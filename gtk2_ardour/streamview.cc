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
#include <gtkmm2ext/utils.h>

#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/track.h"
#include "ardour/session.h"

#include "pbd/compose.h"

#include "canvas/rectangle.h"
#include "canvas/debug.h"

#include "streamview.h"
#include "global_signals.h"
#include "region_view.h"
#include "route_time_axis.h"
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

StreamView::StreamView (RouteTimeAxisView& tv)
	: _trackview (tv)
	, _canvas_group (new ArdourCanvas::Group (_trackview.canvas_display()))
	, _samples_per_pixel (_trackview.editor().get_current_zoom ())
	, rec_updating(false)
	, rec_active(false)
	, stream_base_color(0xFFFFFFFF)
	, _layers (1)
	, _layer_display (Overlaid)
	, height(tv.height)
	, last_rec_data_frame(0)
{
	CANVAS_DEBUG_NAME (_canvas_group, string_compose ("SV canvas group %1", _trackview.name()));
	
	/* set_position() will position the group */

	canvas_rect = new ArdourCanvas::Rectangle (_canvas_group);
	CANVAS_DEBUG_NAME (canvas_rect, string_compose ("SV canvas rectangle %1", _trackview.name()));
	canvas_rect->set (ArdourCanvas::Rect (0, 0, ArdourCanvas::COORD_MAX, tv.current_height ()));
	canvas_rect->raise(1); // raise above tempo lines

	canvas_rect->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::TOP | ArdourCanvas::Rectangle::BOTTOM));
	canvas_rect->set_outline_color (RGBA_TO_UINT (0, 0, 0, 255));
	canvas_rect->set_fill_color (RGBA_TO_UINT (1.0, 0, 0, 255));
	canvas_rect->set_fill (true);

	canvas_rect->Event.connect (sigc::bind (
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
	if (h < 10.0 || h > 1000.0) {
		return -1;
	}

	if (canvas_rect->y1() == h) {
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

	if (_layer_display == Stacked || _layer_display == Expanded) {
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

	RegionViewRemoved (); /* EMIT SIGNAL */
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

	/* draw it */

	redisplay_track ();

	/* update layers count and the y positions and heights of our regions */
	_layers = tr->playlist()->top_layer() + 1;
	update_contents_height ();
	update_coverage_frames ();

	/* catch changes */

	tr->playlist()->LayeringChanged.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::playlist_layered, this, boost::weak_ptr<Track> (tr)), gui_context());
	tr->playlist()->RegionAdded.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::add_region_view, this, _1), gui_context());
	tr->playlist()->RegionRemoved.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::remove_region_view, this, _1), gui_context());
	tr->playlist()->ContentsChanged.connect (playlist_connections, invalidator (*this), boost::bind (&StreamView::update_coverage_frames, this), gui_context());
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
StreamView::apply_color (Gdk::Color color, ColorTarget target)
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
		stream_base_color = RGBA_TO_UINT (color.get_red_p(), color.get_green_p(), color.get_blue_p(), 255);
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
StreamView::update_rec_box ()
{
	if (rec_active && rec_rects.size() > 0) {
		/* only update the last box */
		RecBoxInfo & rect = rec_rects.back();
		framepos_t const at = _trackview.track()->current_capture_end ();
		double xstart;
		double xend;

		switch (_trackview.track()->mode()) {

		case NonLayered:
		case Normal:
			rect.length = at - rect.start;
			xstart = _trackview.editor().sample_to_pixel (rect.start);
			xend = _trackview.editor().sample_to_pixel (at);
			break;

		case Destructive:
			rect.length = 2;
			xstart = _trackview.editor().sample_to_pixel (_trackview.track()->current_capture_start());
			xend = _trackview.editor().sample_to_pixel (at);
			break;
		}

		rect.rectangle->set_x0 (xstart);
		rect.rectangle->set_x1 (xend);
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

		if ((*i)->region()->coverage (start, end) != Evoral::OverlapNone && layer_ok) {
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
	switch (_layer_display) {
	case Overlaid:
		return height;
	case Stacked:
		return height / _layers;
	case Expanded:
		return height / (_layers * 2 + 1);
	}
	
	/* NOTREACHED */
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
	update_coverage_frames ();
}

void
StreamView::update_coverage_frames ()
{
	for (RegionViewList::iterator i = region_views.begin (); i != region_views.end (); ++i) {
		(*i)->update_coverage_frames (_layer_display);
	}
}

void
StreamView::check_record_layers (boost::shared_ptr<Region> region, framepos_t to)
{
	if (_new_rec_layer_time < to) {
		/* The region being recorded has overlapped the start of a top-layered region, so
		   `fake' a new visual layer for the recording.  This is only a visual thing for now,
		   as the proper layering will get sorted out when the recorded region is added to
		   its playlist.
		*/

		/* Stop this happening again */
		_new_rec_layer_time = max_framepos;

		/* Make space in the view for the new layer */
		++_layers;

		/* Set the temporary region to the correct layer so that it gets drawn correctly */
		region->set_layer (_layers - 1);

		/* and reset the view */
		update_contents_height ();
	}
}

void
StreamView::setup_new_rec_layer_time (boost::shared_ptr<Region> region)
{
	/* If we are in Stacked mode, we may need to (visually) create a new layer to put the
	   recorded region in.  To work out where this needs to happen, find the start of the next
	   top-layered region after the start of the region we are recording and make a note of it.
	*/
	if (_layer_display == Stacked) {
		_new_rec_layer_time = _trackview.track()->playlist()->find_next_top_layer_position (region->start());
	} else {
		_new_rec_layer_time = max_framepos;
	}
}

void
StreamView::enter_internal_edit_mode ()
{
        for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
                (*i)->hide_rect ();
        }
}

void
StreamView::leave_internal_edit_mode ()
{
        for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
                (*i)->show_rect ();
        }
}
