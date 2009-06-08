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
#include "ardour/diskstream.h"
#include "ardour/track.h"

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

StreamView::StreamView (RouteTimeAxisView& tv, ArdourCanvas::Group* group)
	: _trackview (tv)
	, owns_canvas_group(group == 0)
	, _background_group (new ArdourCanvas::Group (*_trackview.canvas_background()))
	, canvas_group(group ? group : new ArdourCanvas::Group(*_trackview.canvas_display()))
	, _samples_per_unit (_trackview.editor().get_current_zoom ())
	, rec_updating(false)
	, rec_active(false)
	, use_rec_regions (tv.editor().show_waveforms_recording ())
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
	canvas_rect->property_x2() = _trackview.editor().get_physical_screen_width ();
	canvas_rect->property_y2() = (double) tv.current_height();
	canvas_rect->raise(1); // raise above tempo lines

	canvas_rect->property_outline_what() = (guint32) (0x2|0x8);  // outline RHS and bottom 

	canvas_rect->signal_event().connect (bind (
			mem_fun (_trackview.editor(), &PublicEditor::canvas_stream_view_event),
			canvas_rect, &_trackview));

	if (_trackview.is_track()) {
		_trackview.track()->DiskstreamChanged.connect (
				mem_fun (*this, &StreamView::diskstream_changed));
		_trackview.session().TransportStateChange.connect (
				mem_fun (*this, &StreamView::transport_changed));
		_trackview.session().TransportLooped.connect (
				mem_fun (*this, &StreamView::transport_looped));
		_trackview.get_diskstream()->RecordEnableChanged.connect (
				mem_fun (*this, &StreamView::rec_enable_changed));
		_trackview.session().RecordStateChanged.connect (
				mem_fun (*this, &StreamView::sess_rec_enable_changed));
	} 

	ColorsChanged.connect (mem_fun (*this, &StreamView::color_handler));
}

StreamView::~StreamView ()
{
	undisplay_diskstream ();
	
	delete canvas_rect;
	
	if (owns_canvas_group) {
		delete canvas_group;
	}
}

void
StreamView::attach ()
{
	if (_trackview.is_track()) {
		display_diskstream (_trackview.get_diskstream());
	}
}

int
StreamView::set_position (gdouble x, gdouble y)
{
	canvas_group->property_x() = x;
	canvas_group->property_y() = y;
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
StreamView::add_region_view_weak (boost::weak_ptr<Region> r)
{
	boost::shared_ptr<Region> sp (r.lock());
	
	if (sp) {
		add_region_view (sp);
	}
}


void
StreamView::add_region_view (boost::shared_ptr<Region> r)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &StreamView::add_region_view), r));
	
	add_region_view_internal (r, true);
	
	if (_layer_display == Stacked) {
		update_contents_height ();
	}
}

void
StreamView::remove_region_view (boost::weak_ptr<Region> weak_r)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &StreamView::remove_region_view), weak_r));

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
StreamView::undisplay_diskstream ()
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
StreamView::display_diskstream (boost::shared_ptr<Diskstream> ds)
{
	playlist_change_connection.disconnect();
	playlist_changed (ds);
	playlist_change_connection = ds->PlaylistChanged.connect (bind (
			mem_fun (*this, &StreamView::playlist_changed_weak),
			boost::weak_ptr<Diskstream> (ds)));
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

	// Fix canvas layering by raising each in the sorted list order
	for (RegionViewList::iterator i = copy.begin(); i != copy.end(); ++i) {
		region_layered (*i);
	}
}

void
StreamView::playlist_modified_weak (boost::weak_ptr<Diskstream> ds)
{
	boost::shared_ptr<Diskstream> sp (ds.lock());
	
	if (sp) {
		playlist_modified (sp);
	}
}

void
StreamView::playlist_modified (boost::shared_ptr<Diskstream> ds)
{
	/* we do not allow shared_ptr<T> to be bound to slots */
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &StreamView::playlist_modified_weak), ds));

	/* update layers count and the y positions and heights of our regions */
	if (ds->playlist()) {
		_layers = ds->playlist()->top_layer() + 1;
		update_contents_height ();
		update_coverage_frames ();
		//redisplay_diskstream ();
	}
}

void
StreamView::playlist_changed_weak (boost::weak_ptr<Diskstream> ds)
{
	boost::shared_ptr<Diskstream> sp (ds.lock());
	if (sp) {
		playlist_changed (sp);
	}
}

void
StreamView::playlist_changed (boost::shared_ptr<Diskstream> ds)
{
	ENSURE_GUI_THREAD (bind (
			mem_fun (*this, &StreamView::playlist_changed_weak),
			boost::weak_ptr<Diskstream> (ds)));

	/* disconnect from old playlist */

	for (vector<sigc::connection>::iterator i = playlist_connections.begin();
			i != playlist_connections.end(); ++i) {
		(*i).disconnect();
	}
	
	playlist_connections.clear();
	undisplay_diskstream ();

	/* update layers count and the y positions and heights of our regions */
	_layers = ds->playlist()->top_layer() + 1;
	update_contents_height ();

	update_coverage_frames ();
	
	/* draw it */
	redisplay_diskstream ();

	/* catch changes */

	playlist_connections.push_back (ds->playlist()->Modified.connect (bind (
			mem_fun (*this, &StreamView::playlist_modified_weak), ds)));

	playlist_connections.push_back (ds->playlist()->RegionAdded.connect (
			mem_fun (*this, &StreamView::add_region_view_weak)));

	playlist_connections.push_back (ds->playlist()->RegionRemoved.connect (
			mem_fun (*this, &StreamView::remove_region_view)));
}

void
StreamView::diskstream_changed ()
{
	boost::shared_ptr<Track> t;

	if ((t = _trackview.track()) != 0) {
		Gtkmm2ext::UI::instance()->call_slot (bind (
				mem_fun (*this, &StreamView::display_diskstream),
				t->diskstream()));
	} else {
		Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &StreamView::undisplay_diskstream));
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
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &StreamView::setup_rec_box));
}

void
StreamView::sess_rec_enable_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &StreamView::setup_rec_box));
}

void
StreamView::transport_changed()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &StreamView::setup_rec_box));
}

void
StreamView::transport_looped()
{
	// to force a new rec region
	rec_active = false;
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &StreamView::setup_rec_box));
}

void
StreamView::update_rec_box ()
{
	if (rec_active && rec_rects.size() > 0) {
		/* only update the last box */
		RecBoxInfo & rect = rec_rects.back();
		nframes_t at = _trackview.get_diskstream()->current_capture_end();
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
			xstart = _trackview.editor().frame_to_pixel (_trackview.get_diskstream()->current_capture_start());
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
	
void
StreamView::foreach_regionview (sigc::slot<void,RegionView*> slot)
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		slot (*i);
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

void
StreamView::get_selectables (nframes_t start, nframes_t end, list<Selectable*>& results)
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region()->coverage(start, end) != OverlapNone) {
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
		i->rectangle->property_y2() = height - 1.0;
	}
}

void
StreamView::set_layer_display (LayerDisplay d)
{
	_layer_display = d;
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
