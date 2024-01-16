/*
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2007 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2014-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cmath>

#include <gtkmm.h>

#include <gtkmm2ext/colors.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>

#include "ardour/region.h"
#include "ardour/track.h"

#include "pbd/compose.h"

#include "canvas/rectangle.h"
#include "canvas/debug.h"

#include "audio_region_view.h"
#include "streamview.h"
#include "region_view.h"
#include "route_time_axis.h"
#include "region_gain_line.h"
#include "region_selection.h"
#include "selection.h"
#include "public_editor.h"
#include "timers.h"
#include "gui_thread.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

StreamView::StreamView (RouteTimeAxisView& tv, ArdourCanvas::Container* canvas_group)
	: _trackview (tv)
	, _canvas_group (canvas_group ? canvas_group : new ArdourCanvas::Container (_trackview.canvas_display()))
	, _samples_per_pixel (_trackview.editor().get_current_zoom ())
	, rec_updating(false)
	, rec_active(false)
	, stream_base_color(0xFFFFFFFF)
	, _layers (1)
	, _layer_display (Overlaid)
	, height (tv.height)
	, last_rec_data_sample(0)
{
	CANVAS_DEBUG_NAME (_canvas_group, string_compose ("SV canvas group %1", _trackview.name()));

	/* set_position() will position the group */

	canvas_rect = new ArdourCanvas::Rectangle (_canvas_group);
	CANVAS_DEBUG_NAME (canvas_rect, string_compose ("SV canvas rectangle %1", _trackview.name()));
	canvas_rect->set (ArdourCanvas::Rect (0, 0, ArdourCanvas::COORD_MAX, tv.current_height ()));
	canvas_rect->set_outline (false);
	canvas_rect->set_fill (true);
	canvas_rect->Event.connect (sigc::bind (sigc::mem_fun (_trackview.editor(), &PublicEditor::canvas_stream_view_event), canvas_rect, &_trackview));

	if (_trackview.is_track()) {
		_trackview.track()->rec_enable_control()->Changed.connect (*this, invalidator (*this), boost::bind (&StreamView::rec_enable_changed, this), gui_context());

		_trackview.session()->TransportStateChange.connect (*this, invalidator (*this), boost::bind (&StreamView::transport_changed, this), gui_context());
		_trackview.session()->TransportLooped.connect (*this, invalidator (*this), boost::bind (&StreamView::transport_looped, this), gui_context());
		_trackview.session()->RecordStateChanged.connect (*this, invalidator (*this), boost::bind (&StreamView::sess_rec_enable_changed, this), gui_context());
	}

	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &StreamView::color_handler));
}

StreamView::~StreamView ()
{
	delete canvas_rect;
}

void
StreamView::attach ()
{
	if (_trackview.is_track()) {
		display_track (_trackview.track ());
	}
}

int
StreamView::set_position (gdouble x, gdouble y)
{
	_canvas_group->set_position (ArdourCanvas::Duple (x, y));
	return 0;
}

int
StreamView::set_height (double h)
{
	/* limit the values to something sane-ish */

	if (h < 10.0 || h > 2500.0) {
		return -1;
	}

	if (height == h) {
		return 0;
	}

	height = h;
	canvas_rect->set_y1 (height);
	update_contents_height ();

	return 0;
}

int
StreamView::set_samples_per_pixel (double fpp)
{
	RegionViewList::iterator i;

	if (fpp < 1.0) {
		return -1;
	}

	if (fpp == _samples_per_pixel) {
		return 0;
	}

	_samples_per_pixel = fpp;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_samples_per_pixel (fpp);
	}

	for (vector<RecBoxInfo>::iterator xi = rec_rects.begin(); xi != rec_rects.end(); ++xi) {
		RecBoxInfo &recbox = (*xi);

		ArdourCanvas::Coord const xstart = _trackview.editor().sample_to_pixel (recbox.start);
		ArdourCanvas::Coord const xend = _trackview.editor().sample_to_pixel (recbox.start + recbox.length);

		recbox.rectangle->set_x0 (xstart);
		recbox.rectangle->set_x1 (xend);
	}

	update_coverage_frame ();

	return 0;
}

void
StreamView::add_region_view (std::weak_ptr<Region> wr)
{
	std::shared_ptr<Region> r (wr.lock());
	if (!r) {
		return;
	}

	add_region_view_internal (r, true);

	if (_layer_display == Stacked || _layer_display == Expanded) {
		update_contents_height ();
	}
}

void
StreamView::remove_region_view (std::weak_ptr<Region> weak_r)
{
	ENSURE_GUI_THREAD (*this, &StreamView::remove_region_view, weak_r)

	std::shared_ptr<Region> r (weak_r.lock());

	if (!r) {
		return;
	}

	bool clear_rec_rects = false;
	for (list<pair<std::shared_ptr<Region>,RegionView*> >::iterator i = rec_regions.begin(); i != rec_regions.end();) {
		if (i->first == r) {
			i = rec_regions.erase (i);
			clear_rec_rects = true;
		} else {
			++i;
		}
	}

	if (clear_rec_rects) {
		for (auto const& i : rec_rects) {
			delete i.rectangle;
		}
		rec_rects.clear();
	}

	for (list<RegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if (((*i)->region()) == r) {
			RegionView* rv = *i;
			region_views.erase (i);
			delete rv;
			break;
		}
	}

	RegionViewRemoved (); /* EMIT SIGNAL */
}

void
StreamView::undisplay_track ()
{
	RegionViewList copy (region_views);
	region_views.clear ();

	for (auto const & rv : copy) {
		delete rv;
	}
}

void
StreamView::display_track (std::shared_ptr<Track> tr)
{
	playlist_switched_connection.disconnect();
	playlist_switched (tr);
	tr->PlaylistChanged.connect (playlist_switched_connection, invalidator (*this), boost::bind (&StreamView::playlist_switched, this, std::weak_ptr<Track> (tr)), gui_context());
}

void
StreamView::layer_regions()
{
	// In one traversal of the region view list:
	// - Build a list of region views sorted by layer
	// - Remove invalid views from the actual region view list
	RegionViewList copy;
	list<RegionView*>::iterator i, tmp;
	for (i = region_views.begin(); i != region_views.end(); ) {
		tmp = i;
		tmp++;

		if (!(*i)->is_valid()) {
			delete *i;
			region_views.erase (i);
			i = tmp;
			continue;
		}

		if (copy.size() == 0) {
			copy.push_front((*i));
			i = tmp;
			continue;
		}

		RegionViewList::iterator k = copy.begin();
		RegionViewList::iterator l = copy.end();
		l--;

		if ((*i)->region()->layer() <= (*k)->region()->layer()) {
			copy.push_front((*i));
			i = tmp;
			continue;
		} else if ((*i)->region()->layer() >= (*l)->region()->layer()) {
			copy.push_back((*i));
			i = tmp;
			continue;
		}

		for (RegionViewList::iterator j = copy.begin(); j != copy.end(); ++j) {
			if ((*j)->region()->layer() >= (*i)->region()->layer()) {
				copy.insert(j, (*i));
				break;
			}
		}

		i = tmp;
	}

	// Fix canvas layering by raising each to the top in the sorted order.
	for (RegionViewList::iterator i = copy.begin(); i != copy.end(); ++i) {
		(*i)->visual_layer_on_top ();
	}
}

void
StreamView::playlist_layered (std::weak_ptr<Track> wtr)
{
	std::shared_ptr<Track> tr (wtr.lock());

	if (!tr) {
		return;
	}

	/* update layers count and the y positions and heights of our regions */
	if (tr->playlist()) {
		_layers = tr->playlist()->top_layer() + 1;
	}

	if (_layer_display == Stacked) {
		update_contents_height ();
		/* tricky. playlist_changed() does this as well, and its really inefficient. */
		update_coverage_frame ();
	} else {
		/* layering has probably been modified. reflect this in the canvas. */
		layer_regions();
	}
}

void
StreamView::playlist_switched (std::weak_ptr<Track> wtr)
{
	std::shared_ptr<Track> tr (wtr.lock());

	if (!tr) {
		return;
	}

	/* disconnect from old playlist */

	playlist_connections.drop_connections ();
	undisplay_track ();

	/* draw it */
	tr->playlist()->freeze();
	redisplay_track ();
	tr->playlist()->thaw();
	/* update layers count and the y positions and heights of our regions */
	_layers = tr->playlist()->top_layer() + 1;
	update_contents_height ();
	update_coverage_frame ();

	/* catch changes */

	tr->playlist()->LayeringChanged.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::playlist_layered, this, std::weak_ptr<Track> (tr)), gui_context());
	tr->playlist()->RegionAdded.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::add_region_view, this, _1), gui_context());
	tr->playlist()->RegionRemoved.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::remove_region_view, this, _1), gui_context());
	tr->playlist()->ContentsChanged.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::update_coverage_frame, this), gui_context());
}

void
StreamView::apply_color (Gdk::Color const& c, ColorTarget target)
{
	return apply_color (Gtkmm2ext::gdk_color_to_rgba (c), target);
}

void
StreamView::apply_color (uint32_t color, ColorTarget target)
{
	list<RegionView *>::iterator i;

	switch (target) {
	case RegionColor:
		region_color = color;
		for (i = region_views.begin(); i != region_views.end(); ++i) {
			(*i)->set_color (region_color);
		}
		break;

	case StreamBaseColor:
		stream_base_color = color;
		canvas_rect->set_fill_color (stream_base_color);
		break;
	}
}

void
StreamView::region_layered (RegionView* rv)
{
	/* don't ever leave it at the bottom, since then it doesn't
	   get events - the  parent group does instead ...
	*/
	rv->get_canvas_group()->raise (rv->region()->layer());
}

void
StreamView::rec_enable_changed ()
{
	setup_rec_box ();
}

void
StreamView::sess_rec_enable_changed ()
{
	setup_rec_box ();
}

void
StreamView::transport_changed()
{
	setup_rec_box ();
}

void
StreamView::transport_looped()
{
	// to force a new rec region
	rec_active = false;
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&StreamView::setup_rec_box, this));
}

void
StreamView::create_rec_box(samplepos_t sample_pos, double width)
{
	const double   xstart     = _trackview.editor().sample_to_pixel(sample_pos);
	const double   xend       = xstart + width;
	const uint32_t fill_color = UIConfiguration::instance().color_mod("recording rect", "recording_rect");

	ArdourCanvas::Rectangle* rec_rect = new ArdourCanvas::Rectangle(_canvas_group);
	rec_rect->set_x0(xstart);
	rec_rect->set_y0(0);
	rec_rect->set_x1(xend);
	rec_rect->set_y1(child_height ());
	rec_rect->set_outline_what(ArdourCanvas::Rectangle::What(0));
	rec_rect->set_outline_color(UIConfiguration::instance().color("recording rect"));
	rec_rect->set_fill_color(fill_color);
	rec_rect->lower_to_bottom();

	RecBoxInfo recbox;
	recbox.rectangle = rec_rect;
	recbox.length    = 0;

	if (rec_rects.empty()) {
		recbox.start = _trackview.session()->record_location ();
	} else {
		recbox.start = _trackview.session()->transport_sample ();
	}

	rec_rects.push_back (recbox);

	screen_update_connection.disconnect();
	screen_update_connection = Timers::rapid_connect (sigc::mem_fun(*this, &StreamView::update_rec_box));

	rec_updating = true;
	rec_active = true;
}

void
StreamView::update_rec_box ()
{
	if (rec_active && rec_rects.size() > 0) {
		/* only update the last box */
		RecBoxInfo & rect = rec_rects.back();
		samplepos_t const at = _trackview.track()->current_capture_end ();
		double xstart;
		double xend;

		switch (_trackview.track()->mode()) {

		case NonLayered:
		case Normal:
			rect.length = at - rect.start;
			xstart = _trackview.editor().sample_to_pixel (rect.start);
			xend = _trackview.editor().sample_to_pixel (at);
			break;

		default:
			fatal << string_compose (_("programming error: %1"), "illegal track mode") << endmsg;
			abort(); /*NOTREACHED*/
			return;
		}

		rect.rectangle->set_x0 (xstart);
		rect.rectangle->set_x1 (xend);
	}
}

void
StreamView::cleanup_rec_box ()
{
	if (rec_rects.empty() && rec_regions.empty()) {
		return;
	}

	/* disconnect rapid update */
	screen_update_connection.disconnect();
	rec_data_ready_connections.drop_connections ();
	rec_updating = false;
	rec_active = false;

	/* remove temp regions */
	auto rr (rec_regions);
	for (auto const& i : rr) {
		i.first->drop_references ();
	}

	rec_regions.clear();

	// cerr << "\tclear " << rec_rects.size() << " rec rects\n";

	/* transport stopped, clear boxes */
	for (auto const& i : rec_rects) {
		delete i.rectangle;
	}
	rec_rects.clear();
}

RegionView*
StreamView::find_view (std::shared_ptr<const Region> region)
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {

		if ((*i)->region() == region) {
			return *i;
		}
	}
	return 0;
}

uint32_t
StreamView::num_selected_regionviews () const
{
	uint32_t cnt = 0;

	for (list<RegionView*>::const_iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->selected()) {
			++cnt;
		}
	}
	return cnt;
}

void
StreamView::foreach_regionview (sigc::slot<void,RegionView*> slot)
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		slot (*i);
	}
}

void
StreamView::foreach_selected_regionview (sigc::slot<void,RegionView*> slot)
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->selected()) {
			slot (*i);
		}
	}
}

void
StreamView::set_selected_regionviews (RegionSelection& regions)
{
	bool selected;

	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {

		selected = false;

		/* Linear search: probably as good as anything else */

		for (RegionSelection::iterator ii = regions.begin(); ii != regions.end(); ++ii) {
			if ((*i)->region() == (*ii)->region()) {
				selected = true;
				break;
			}
		}

		(*i)->set_selected (selected);
	}
}

/** Get selectable things within a given range.
 *  @param start Start time in session samples.
 *  @param end End time in session samples.
 *  @param top Top y range, in trackview coordinates (ie 0 is the top of the track view)
 *  @param bot Bottom y range, in trackview coordinates (ie 0 is the top of the track view)
 *  @param result Filled in with selectable things.
 */
void
StreamView::get_selectables (timepos_t const & start, timepos_t const & end, double top, double bottom, list<Selectable*>& results, bool within)
{
	layer_t min_layer = 0;
	layer_t max_layer = 0;

	if (_layer_display == Stacked) {
		double const c = child_height ();

		int const mi = _layers - ((bottom - _trackview.y_position()) / c);
		if (mi < 0) {
			min_layer = 0;
		} else {
			min_layer = mi;
		}

		int const ma = _layers - ((top - _trackview.y_position()) / c);
		if (ma > (int) _layers) {
			max_layer = _layers - 1;
		} else {
			max_layer = ma;
		}

	}

	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {

		bool layer_ok = true;

		if (_layer_display == Stacked) {
			layer_t const l = (*i)->region()->layer ();
			layer_ok = (min_layer <= l && l <= max_layer);
		}
		if (!layer_ok) {
			continue;
		}
		if ((within && (*i)->region()->coverage (start, end) == Temporal::OverlapExternal)
		    || (!within && (*i)->region()->coverage (start, end) != Temporal::OverlapNone)) {
			if (_trackview.editor().internal_editing()) {
				AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*i);
				if (arv && arv->fx_line ()) {
					/* Note: AutomationLine::get_selectables() uses trackview.current_height (),
					 * disregarding Stacked layer display height
					 */
					double const c = height; // child_height (); // XXX
					double const y = (*i)->get_canvas_group ()->position().y;
					double t = 1.0 - std::min (1.0, std::max (0., (top - _trackview.y_position () - y) / c));
					double b = 1.0 - std::min (1.0, std::max (0., (bottom - _trackview.y_position () - y) / c));
					arv->fx_line()->get_selectables (start, end, b, t, results);
				}
			} else {
				results.push_back (*i);
			}
		}
	}
}

void
StreamView::get_inverted_selectables (Selection& sel, list<Selectable*>& results)
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if (!sel.regions.contains (*i)) {
			results.push_back (*i);
		}
	}
}

void
StreamView::get_regionviews_at_or_after (timepos_t const & pos, RegionSelection& regions)
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region()->position() >= pos) {
			regions.push_back (*i);
		}
	}
}

/** @return height of a child region view, depending on stacked / overlaid mode */
double
StreamView::child_height () const
{
	switch (_layer_display) {
	case Overlaid:
		return height;
	case Stacked:
		return height / _layers;
	case Expanded:
		return height / (_layers * 2 + 1);
	}

	abort(); /* NOTREACHED */
	return height;
}

void
StreamView::update_contents_height ()
{
	const double h = child_height ();

	for (RegionViewList::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		switch (_layer_display) {
		case Overlaid:
			(*i)->set_y (0);
			break;
		case Stacked:
			(*i)->set_y (height - ((*i)->region()->layer() + 1) * h);
			break;
		case Expanded:
			(*i)->set_y (height - ((*i)->region()->layer() + 1) * 2 * h);
			break;
		}

		(*i)->set_height (h);
	}

	for (vector<RecBoxInfo>::iterator i = rec_rects.begin(); i != rec_rects.end(); ++i) {
		switch (_layer_display) {
		case Overlaid:
			i->rectangle->set_y1 (height);
			break;
		case Stacked:
		case Expanded:
			/* In stacked displays, the recregion is always at the top */
			i->rectangle->set_y0 (0);
			i->rectangle->set_y1 (h);
			break;
		}
	}

	ContentsHeightChanged (); /* EMIT SIGNAL */
}

void
StreamView::set_layer_display (LayerDisplay d)
{
	_layer_display = d;

	if (_layer_display == Overlaid) {
		layer_regions ();
	}

	update_contents_height ();
	update_coverage_frame ();
}

void
StreamView::update_coverage_frame ()
{
	for (RegionViewList::iterator i = region_views.begin (); i != region_views.end (); ++i) {
		(*i)->update_coverage_frame (_layer_display);
	}
}

void
StreamView::check_record_layers (std::shared_ptr<Region> region, samplepos_t to)
{
	if (_new_rec_layer_time < to) {
		/* The region being recorded has overlapped the start of a top-layered region, so
		   `fake' a new visual layer for the recording.  This is only a visual thing for now,
		   as the proper layering will get sorted out when the recorded region is added to
		   its playlist.
		*/

		/* Stop this happening again */
		_new_rec_layer_time = max_samplepos;

		/* Make space in the view for the new layer */
		++_layers;

		/* Set the temporary region to the correct layer so that it gets drawn correctly */
		region->set_layer (_layers - 1);

		/* and reset the view */
		update_contents_height ();
	}
}

void
StreamView::setup_new_rec_layer_time (std::shared_ptr<Region> region)
{
	/* If we are in Stacked mode, we may need to (visually) create a new layer to put the
	   recorded region in.  To work out where this needs to happen, find the start of the next
	   top-layered region after the start of the region we are recording and make a note of it.
	*/
	if (_layer_display == Stacked) {
		_new_rec_layer_time = _trackview.track()->playlist()->find_next_top_layer_position (region->position()).samples();
	} else {
		_new_rec_layer_time = max_samplepos;
	}
}

void
StreamView::parameter_changed (string const & what)
{
	if (what == "show-region-name") {
		for (RegionViewList::iterator i = region_views.begin (); i != region_views.end (); ++i) {
			(*i)->update_visibility ();
		}
	}
}
