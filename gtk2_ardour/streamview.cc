/*
    Copyright (C) 2001, 2006 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/source.h"
#include "ardour/track.h"
#include "ardour/session.h"

#include "streamview.h"
#include "region_view.h"
#include "route_time_axis.h"
#include "canvas-waveview.h"
#include "canvas-simplerect.h"
#include "region_selection.h"
#include "selection.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "utils.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

StreamView::StreamView (RouteTimeAxisView& tv, ArdourCanvas::Group* background_group, ArdourCanvas::Group* canvas_group)
	: _trackview (tv)
	, owns_background_group (background_group == 0)
	, owns_canvas_group (canvas_group == 0)
	, _background_group (background_group ? background_group : new ArdourCanvas::Group (*_trackview.canvas_background()))
	, _canvas_group (canvas_group ? canvas_group : new ArdourCanvas::Group(*_trackview.canvas_display()))
	, _samples_per_unit (_trackview.editor().get_current_zoom ())
	, rec_updating(false)
	, rec_active(false)
	, region_color(_trackview.color())
	, stream_base_color(0xFFFFFFFF)
	, _layers (1)
	, _layer_display (Overlaid)
	, height(tv.height)
	, last_rec_data_frame(0)
{
	/* set_position() will position the group */

	canvas_rect = new ArdourCanvas::SimpleRect (*_background_group);
	canvas_rect->property_x1() = 0.0;
	canvas_rect->property_y1() = 0.0;
	canvas_rect->property_x2() = physical_screen_width (_trackview.editor().get_window());
	canvas_rect->property_y2() = (double) tv.current_height();
	canvas_rect->raise(1); // raise above tempo lines

	canvas_rect->property_outline_what() = (guint32) (0x2|0x8);  // outline RHS and bottom

	canvas_rect->signal_event().connect (sigc::bind (
			sigc::mem_fun (_trackview.editor(), &PublicEditor::canvas_stream_view_event),
			canvas_rect, &_trackview));

	if (_trackview.is_track()) {
		_trackview.track()->DiskstreamChanged.connect (*this, invalidator (*this), boost::bind (&StreamView::diskstream_changed, this), gui_context());
		_trackview.track()->RecordEnableChanged.connect (*this, invalidator (*this), boost::bind (&StreamView::rec_enable_changed, this), gui_context());

		_trackview.session()->TransportStateChange.connect (*this, invalidator (*this), boost::bind (&StreamView::transport_changed, this), gui_context());
		_trackview.session()->TransportLooped.connect (*this, invalidator (*this), boost::bind (&StreamView::transport_looped, this), gui_context());
		_trackview.session()->RecordStateChanged.connect (*this, invalidator (*this), boost::bind (&StreamView::sess_rec_enable_changed, this), gui_context());
	}

	ColorsChanged.connect (sigc::mem_fun (*this, &StreamView::color_handler));
}

StreamView::~StreamView ()
{
	undisplay_track ();

	delete canvas_rect;

	if (owns_background_group) {
		delete _background_group;
	}
	
	if (owns_canvas_group) {
		delete _canvas_group;
	}
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
	_canvas_group->property_x() = x;
	_canvas_group->property_y() = y;
	return 0;
}

int
StreamView::set_height (double h)
{
	/* limit the values to something sane-ish */
	if (h < 10.0 || h > 1000.0) {
		return -1;
	}

	if (canvas_rect->property_y2() == h) {
		return 0;
	}

	height = h;
	canvas_rect->property_y2() = height;
	update_contents_height ();

	return 0;
}

int
StreamView::set_samples_per_unit (gdouble spp)
{
	RegionViewList::iterator i;

	if (spp < 1.0) {
		return -1;
	}

	_samples_per_unit = spp;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_samples_per_unit (spp);
	}

	for (vector<RecBoxInfo>::iterator xi = rec_rects.begin(); xi != rec_rects.end(); ++xi) {
		RecBoxInfo &recbox = (*xi);

		gdouble xstart = _trackview.editor().frame_to_pixel (recbox.start);
		gdouble xend = _trackview.editor().frame_to_pixel (recbox.start + recbox.length);

		recbox.rectangle->property_x1() = xstart;
		recbox.rectangle->property_x2() = xend;
	}

	update_coverage_frames ();

	return 0;
}

void
StreamView::add_region_view (boost::weak_ptr<Region> wr)
{
	boost::shared_ptr<Region> r (wr.lock());
	if (!r) {
		return;
	}

	add_region_view_internal (r, true);

	if (_layer_display == Stacked) {
		update_contents_height ();
	}
}

void
StreamView::remove_region_view (boost::weak_ptr<Region> weak_r)
{
	ENSURE_GUI_THREAD (*this, &StreamView::remove_region_view, weak_r)

	boost::shared_ptr<Region> r (weak_r.lock());

	if (!r) {
		return;
	}

	for (list<RegionView *>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if (((*i)->region()) == r) {
			RegionView* rv = *i;
			region_views.erase (i);
			delete rv;
			break;
		}
	}
}

void
StreamView::undisplay_track ()
{
	for (RegionViewList::iterator i = region_views.begin(); i != region_views.end() ; ) {
		RegionViewList::iterator next = i;
		++next;
		delete *i;
		i = next;
	}

	region_views.clear();
}

void
StreamView::display_track (boost::shared_ptr<Track> tr)
{
	playlist_switched_connection.disconnect();
	playlist_switched (tr);
	tr->PlaylistChanged.connect (playlist_switched_connection, invalidator (*this), boost::bind (&StreamView::playlist_switched, this, boost::weak_ptr<Track> (tr)), gui_context());
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
		} else {
			(*i)->enable_display(true);
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
		(*i)->get_canvas_group()->raise_to_top ();
	}
}

void
StreamView::playlist_layered (boost::weak_ptr<Track> wtr)
{
	boost::shared_ptr<Track> tr (wtr.lock());

	if (!tr) {
		return;
	}

	/* update layers count and the y positions and heights of our regions */
	if (tr->playlist()) {
		_layers = tr->playlist()->top_layer() + 1;
	}

	if (_layer_display == Stacked) {
		update_contents_height ();
		update_coverage_frames ();
	} else {
		/* layering has probably been modified. reflect this in the canvas. */
		layer_regions();
	} 
}

void
StreamView::playlist_switched (boost::weak_ptr<Track> wtr)
{
	boost::shared_ptr<Track> tr (wtr.lock());

	if (!tr) {
		return;
	}

	/* disconnect from old playlist */

	playlist_connections.drop_connections ();
	undisplay_track ();

	/* update layers count and the y positions and heights of our regions */
	_layers = tr->playlist()->top_layer() + 1;
	update_contents_height ();
	update_coverage_frames ();

	tr->playlist()->set_explicit_relayering (_layer_display == Stacked);

	/* draw it */

	redisplay_track ();

	/* catch changes */

	tr->playlist()->LayeringChanged.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::playlist_layered, this, boost::weak_ptr<Track> (tr)), gui_context());
	tr->playlist()->RegionAdded.connect (playlist_connections, invalidator (*this), ui_bind (&StreamView::add_region_view, this, _1), gui_context());
	tr->playlist()->RegionRemoved.connect (playlist_connections, invalidator (*this), ui_bind (&StreamView::remove_region_view, this, _1), gui_context());
	// ds->playlist()->ContentsChanged.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::redisplay_diskstream, this), gui_context());
}

void
StreamView::diskstream_changed ()
{
	boost::shared_ptr<Track> t;
        
	if ((t = _trackview.track()) != 0) {
		Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&StreamView::display_track, this, t));
	} else {
		Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&StreamView::undisplay_track, this));
	}
}

void
StreamView::apply_color (Gdk::Color& color, ColorTarget target)
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
		stream_base_color = RGBA_TO_UINT (
			color.get_red_p(), color.get_green_p(), color.get_blue_p(), 255);
		canvas_rect->property_fill_color_rgba() = stream_base_color;
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
StreamView::update_rec_box ()
{
	if (rec_active && rec_rects.size() > 0) {
		/* only update the last box */
		RecBoxInfo & rect = rec_rects.back();
		nframes_t at = _trackview.track()->current_capture_end();
		double xstart;
		double xend;

		switch (_trackview.track()->mode()) {

		case NonLayered:
		case Normal:
			rect.length = at - rect.start;
			xstart = _trackview.editor().frame_to_pixel (rect.start);
			xend = _trackview.editor().frame_to_pixel (at);
			break;

		case Destructive:
			rect.length = 2;
			xstart = _trackview.editor().frame_to_pixel (_trackview.track()->current_capture_start());
			xend = _trackview.editor().frame_to_pixel (at);
			break;
		}

		rect.rectangle->property_x1() = xstart;
		rect.rectangle->property_x2() = xend;
	}
}

RegionView*
StreamView::find_view (boost::shared_ptr<const Region> region)
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
		if ((*i)->get_selected()) {
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
		if ((*i)->get_selected()) {
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

		for (RegionSelection::iterator ii = regions.begin(); ii != regions.end(); ++ii) {
			if (*i == *ii) {
				selected = true;
				break;
			}
		}

		(*i)->set_selected (selected);
	}
}


/** Get selectable things within a given range.
 *  @param start Start time in session frames.
 *  @param end End time in session frames.
 *  @param top Top y range, in trackview coordinates (ie 0 is the top of the track view)
 *  @param bot Bottom y range, in trackview coordinates (ie 0 is the top of the track view)
 *  @param result Filled in with selectable things.
 */

void
StreamView::get_selectables (framepos_t start, framepos_t end, double top, double bottom, list<Selectable*>& results)
{
	layer_t min_layer = 0;
	layer_t max_layer = 0;

	if (_layer_display == Stacked) {
		double const c = child_height ();
		min_layer = _layers - ((bottom - _trackview.y_position()) / c);
		max_layer = _layers - ((top - _trackview.y_position()) / c);
	}

	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {

		bool layer_ok = true;

		if (_layer_display == Stacked) {
			layer_t const l = (*i)->region()->layer ();
			layer_ok = (min_layer <= l && l <= max_layer);
		}

		if ((*i)->region()->coverage (start, end) != OverlapNone && layer_ok) {
			results.push_back (*i);
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

/** @return height of a child region view, depending on stacked / overlaid mode */
double
StreamView::child_height () const
{
	if (_layer_display == Stacked) {
		return height / _layers;
	}

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
		}

		(*i)->set_height (h);
	}

	for (vector<RecBoxInfo>::iterator i = rec_rects.begin(); i != rec_rects.end(); ++i) {
		i->rectangle->property_y2() = height;
	}
}

void
StreamView::set_layer_display (LayerDisplay d)
{
	_layer_display = d;
	update_contents_height ();
	update_coverage_frames ();
	_trackview.track()->playlist()->set_explicit_relayering (_layer_display == Stacked);
}

void
StreamView::update_coverage_frames ()
{
	for (RegionViewList::iterator i = region_views.begin (); i != region_views.end (); ++i) {
		(*i)->update_coverage_frames (_layer_display);
	}
}
