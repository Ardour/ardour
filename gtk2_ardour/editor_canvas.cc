/*
    Copyright (C) 2005 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include "gtkmm2ext/utils.h"

#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/smf_source.h"

#include "canvas/canvas.h"
#include "canvas/rectangle.h"
#include "canvas/pixbuf.h"
#include "canvas/text.h"
#include "canvas/debug.h"

#include "ardour_ui.h"
#include "editor.h"
#include "global_signals.h"
#include "editing.h"
#include "rgb_macros.h"
#include "utils.h"
#include "audio_time_axis.h"
#include "editor_drag.h"
#include "region_view.h"
#include "editor_group_tabs.h"
#include "editor_summary.h"
#include "video_timeline.h"
#include "keyboard.h"
#include "editor_cursors.h"
#include "mouse_cursors.h"
#include "verbose_cursor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace Editing;

/* XXX this is a hack. it ought to be the maximum value of an framepos_t */

const double max_canvas_coordinate = (double) UINT32_MAX;

void
Editor::initialize_canvas ()
{
	_track_canvas_viewport = new ArdourCanvas::GtkCanvasViewport (horizontal_adjustment, vertical_adjustment);
	_track_canvas = _track_canvas_viewport->canvas ();

	_time_bars_canvas_viewport = new ArdourCanvas::GtkCanvasViewport (horizontal_adjustment, unused_adjustment);
	_time_bars_canvas = _time_bars_canvas_viewport->canvas ();
	
	_verbose_cursor = new VerboseCursor (this);

	/* on the bottom, an image */

	if (Profile->get_sae()) {
		Image img (::get_icon (X_("saelogo")));
		// logo_item = new ArdourCanvas::Pixbuf (_track_canvas->root(), 0.0, 0.0, img.get_pixbuf());
		// logo_item->property_height_in_pixels() = true;
		// logo_item->property_width_in_pixels() = true;
		// logo_item->property_height_set() = true;
		// logo_item->property_width_set() = true;
		// logo_item->show ();
	}
	
	/*a group to hold global rects like punch/loop indicators */
	global_rect_group = new ArdourCanvas::Group (_track_canvas->root());
	CANVAS_DEBUG_NAME (global_rect_group, "global rect group");

        transport_loop_range_rect = new ArdourCanvas::Rectangle (global_rect_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, ArdourCanvas::COORD_MAX));
	CANVAS_DEBUG_NAME (transport_loop_range_rect, "loop rect");
	transport_loop_range_rect->hide();

	transport_punch_range_rect = new ArdourCanvas::Rectangle (global_rect_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, ArdourCanvas::COORD_MAX));
	CANVAS_DEBUG_NAME (transport_punch_range_rect, "punch rect");
	transport_punch_range_rect->hide();

	/*a group to hold time (measure) lines */
	time_line_group = new ArdourCanvas::Group (_track_canvas->root());
	CANVAS_DEBUG_NAME (time_line_group, "time line group");

	_trackview_group = new ArdourCanvas::Group (_track_canvas->root());
	CANVAS_DEBUG_NAME (_trackview_group, "Canvas TrackViews");
	_region_motion_group = new ArdourCanvas::Group (_trackview_group);
	CANVAS_DEBUG_NAME (_region_motion_group, "Canvas Region Motion");

	meter_bar_group = new ArdourCanvas::Group (_time_bars_canvas->root ());
	meter_bar = new ArdourCanvas::Rectangle (meter_bar_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (meter_bar, "meter Bar");
	meter_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	tempo_bar_group = new ArdourCanvas::Group (_time_bars_canvas->root ());
	tempo_bar = new ArdourCanvas::Rectangle (tempo_bar_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (tempo_bar, "Tempo  Bar");
	tempo_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	range_marker_bar_group = new ArdourCanvas::Group (_time_bars_canvas->root ());
	range_marker_bar = new ArdourCanvas::Rectangle (range_marker_bar_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (range_marker_bar, "Range Marker Bar");
	range_marker_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	transport_marker_bar_group = new ArdourCanvas::Group (_time_bars_canvas->root ());
	transport_marker_bar = new ArdourCanvas::Rectangle (transport_marker_bar_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (transport_marker_bar, "transport Marker Bar");
	transport_marker_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	marker_bar_group = new ArdourCanvas::Group (_time_bars_canvas->root ());
	marker_bar = new ArdourCanvas::Rectangle (marker_bar_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (marker_bar, "Marker Bar");
	marker_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	cd_marker_bar_group = new ArdourCanvas::Group (_time_bars_canvas->root ());
	cd_marker_bar = new ArdourCanvas::Rectangle (cd_marker_bar_group, ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (cd_marker_bar, "CD Marker Bar");
 	cd_marker_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);
	
	_time_markers_group = new ArdourCanvas::Group (_time_bars_canvas->root());

	cd_marker_group = new ArdourCanvas::Group (_time_markers_group, ArdourCanvas::Duple (0.0, 0.0));
	CANVAS_DEBUG_NAME (cd_marker_group, "cd marker group");
	/* the vide is temporarily placed a the same location as the
	   cd_marker_group, but is moved later.
	*/
	videotl_group = new ArdourCanvas::Group (_time_markers_group, ArdourCanvas::Duple(0.0, 0.0));
	CANVAS_DEBUG_NAME (videotl_group, "videotl group");
	marker_group = new ArdourCanvas::Group (_time_markers_group, ArdourCanvas::Duple (0.0, timebar_height + 1.0));
	CANVAS_DEBUG_NAME (marker_group, "marker group");
	transport_marker_group = new ArdourCanvas::Group (_time_markers_group, ArdourCanvas::Duple (0.0, (timebar_height * 2.0) + 1.0));
	CANVAS_DEBUG_NAME (transport_marker_group, "transport marker group");
	range_marker_group = new ArdourCanvas::Group (_time_markers_group, ArdourCanvas::Duple (0.0, (timebar_height * 3.0) + 1.0));
	CANVAS_DEBUG_NAME (range_marker_group, "range marker group");
	tempo_group = new ArdourCanvas::Group (_time_markers_group, ArdourCanvas::Duple (0.0, (timebar_height * 4.0) + 1.0));
	CANVAS_DEBUG_NAME (tempo_group, "tempo group");
	meter_group = new ArdourCanvas::Group (_time_markers_group, ArdourCanvas::Duple (0.0, (timebar_height * 5.0) + 1.0));
	CANVAS_DEBUG_NAME (meter_group, "meter group");

	ARDOUR_UI::instance()->video_timeline = new VideoTimeLine(this, videotl_group, (timebar_height * videotl_bar_height));

	cd_marker_bar_drag_rect = new ArdourCanvas::Rectangle (cd_marker_group, ArdourCanvas::Rect (0.0, 0.0, 100, timebar_height));
	CANVAS_DEBUG_NAME (cd_marker_bar_drag_rect, "cd marker drag");
	cd_marker_bar_drag_rect->set_outline (false);
	cd_marker_bar_drag_rect->hide ();

	range_bar_drag_rect = new ArdourCanvas::Rectangle (range_marker_group, ArdourCanvas::Rect (0.0, 0.0, 100, timebar_height));
	CANVAS_DEBUG_NAME (range_bar_drag_rect, "range drag");
	range_bar_drag_rect->set_outline (false);
	range_bar_drag_rect->hide ();

	transport_bar_drag_rect = new ArdourCanvas::Rectangle (transport_marker_group, ArdourCanvas::Rect (0.0, 0.0, 100, timebar_height));
	CANVAS_DEBUG_NAME (transport_bar_drag_rect, "transport drag");
	transport_bar_drag_rect->set_outline (false);
	transport_bar_drag_rect->hide ();

	transport_punchin_line = new ArdourCanvas::Line (_track_canvas->root());
	transport_punchin_line->set_x0 (0);
	transport_punchin_line->set_y0 (0);
	transport_punchin_line->set_x1 (0);
	transport_punchin_line->set_y1 (ArdourCanvas::COORD_MAX);
	transport_punchin_line->hide ();

	transport_punchout_line  = new ArdourCanvas::Line (_track_canvas->root());
	transport_punchout_line->set_x0 (0);
	transport_punchout_line->set_y0 (0);
	transport_punchout_line->set_x1 (0);
	transport_punchout_line->set_y1 (ArdourCanvas::COORD_MAX);
	transport_punchout_line->hide();

	// used to show zoom mode active zooming
	zoom_rect = new ArdourCanvas::Rectangle (_track_canvas->root(), ArdourCanvas::Rect (0.0, 0.0, 0.0, 0.0));
	zoom_rect->hide();
	zoom_rect->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_zoom_rect_event), (ArdourCanvas::Item*) 0));

	// used as rubberband rect
	rubberband_rect = new ArdourCanvas::Rectangle (_trackview_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, 0.0));
	rubberband_rect->hide();

	tempo_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_tempo_bar_event), tempo_bar));
	meter_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_meter_bar_event), meter_bar));
	marker_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_marker_bar_event), marker_bar));
	cd_marker_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_cd_marker_bar_event), cd_marker_bar));
	videotl_group->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_videotl_bar_event), videotl_group));
	range_marker_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_range_marker_bar_event), range_marker_bar));
	transport_marker_bar->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_transport_marker_bar_event), transport_marker_bar));

	playhead_cursor = new EditorCursor (*this, &Editor::canvas_playhead_cursor_event);

	if (logo_item) {
		logo_item->lower_to_bottom ();
	}

	/* these signals will initially be delivered to the canvas itself, but if they end up remaining unhandled, they are passed to Editor-level
	   handlers.
	*/

	_track_canvas->signal_scroll_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_scroll_event));
	_track_canvas->signal_motion_notify_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_motion_notify_event));
	_track_canvas->signal_button_press_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_button_press_event));
	_track_canvas->signal_button_release_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_button_release_event));
	_track_canvas->signal_drag_motion().connect (sigc::mem_fun (*this, &Editor::track_canvas_drag_motion));
	_track_canvas->signal_key_press_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_key_press));
	_track_canvas->signal_key_release_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_key_release));

	_track_canvas->set_name ("EditorMainCanvas");
	_track_canvas->add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	_track_canvas->signal_leave_notify_event().connect (sigc::mem_fun(*this, &Editor::left_track_canvas), false);
	_track_canvas->signal_enter_notify_event().connect (sigc::mem_fun(*this, &Editor::entered_track_canvas), false);
	_track_canvas->set_flags (CAN_FOCUS);

	/* set up drag-n-drop */

	vector<TargetEntry> target_table;

	// Drag-N-Drop from the region list can generate this target
	target_table.push_back (TargetEntry ("regions"));

	target_table.push_back (TargetEntry ("text/plain"));
	target_table.push_back (TargetEntry ("text/uri-list"));
	target_table.push_back (TargetEntry ("application/x-rootwin-drop"));

	_track_canvas->drag_dest_set (target_table);
	_track_canvas->signal_drag_data_received().connect (sigc::mem_fun(*this, &Editor::track_canvas_drag_data_received));

	_track_canvas_viewport->signal_size_allocate().connect (sigc::mem_fun(*this, &Editor::track_canvas_viewport_allocate));

	ColorsChanged.connect (sigc::mem_fun (*this, &Editor::color_handler));
	color_handler();

}

void
Editor::track_canvas_viewport_allocate (Gtk::Allocation alloc)
{
	_canvas_viewport_allocation = alloc;
	track_canvas_viewport_size_allocated ();
}

void
Editor::track_canvas_viewport_size_allocated ()
{
	bool height_changed = _visible_canvas_height != _canvas_viewport_allocation.get_height();

	_visible_canvas_width  = _canvas_viewport_allocation.get_width ();
	_visible_canvas_height = _canvas_viewport_allocation.get_height ();

	// SHOWTRACKS

	if (height_changed) {

		for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
			i->second->canvas_height_set (_visible_canvas_height);
		}

		vertical_adjustment.set_page_size (_visible_canvas_height);
		if ((vertical_adjustment.get_value() + _visible_canvas_height) >= vertical_adjustment.get_upper()) {
			/*
			   We're increasing the size of the canvas while the bottom is visible.
			   We scroll down to keep in step with the controls layout.
			*/
			vertical_adjustment.set_value (_full_canvas_height - _visible_canvas_height);
		}

		set_visible_track_count (_visible_track_count);
	}

	update_fixed_rulers();
	redisplay_tempo (false);
	_summary->set_overlays_dirty ();
}

void
Editor::reset_controls_layout_width ()
{
	GtkRequisition req;
	gint w;

	edit_controls_vbox.size_request (req);
	w = req.width;

        if (_group_tabs->is_mapped()) {
		_group_tabs->size_request (req);
                w += req.width;
        }

        /* the controls layout has no horizontal scrolling, its visible
           width is always equal to the total width of its contents.
        */

        controls_layout.property_width() = w;
        controls_layout.property_width_request() = w;
}

void
Editor::reset_controls_layout_height (int32_t h)
{
        /* set the height of the scrollable area (i.e. the sum of all contained widgets)
         */

        controls_layout.property_height() = h;

        /* size request is set elsewhere, see ::track_canvas_allocate() */
}

bool
Editor::track_canvas_map_handler (GdkEventAny* /*ev*/)
{
	if (current_canvas_cursor) {
		set_canvas_cursor (current_canvas_cursor);
	}
	return false;
}

/** This is called when something is dropped onto the track canvas */
void
Editor::track_canvas_drag_data_received (const RefPtr<Gdk::DragContext>& context,
					 int x, int y,
					 const SelectionData& data,
					 guint info, guint time)
{
	if (data.get_target() == "regions") {
		drop_regions (context, x, y, data, info, time);
	} else {
		drop_paths (context, x, y, data, info, time);
	}
}

bool
Editor::idle_drop_paths (vector<string> paths, framepos_t frame, double ypos, bool copy)
{
	drop_paths_part_two (paths, frame, ypos, copy);
	return false;
}

void
Editor::drop_paths_part_two (const vector<string>& paths, framepos_t frame, double ypos, bool copy)
{
	RouteTimeAxisView* tv;
	
	/* MIDI files must always be imported, because we consider them
	 * writable. So split paths into two vectors, and follow the import
	 * path on the MIDI part.
	 */

	vector<string> midi_paths;
	vector<string> audio_paths;

	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		if (SMFSource::safe_midi_file_extension (*i)) {
			midi_paths.push_back (*i);
		} else {
			audio_paths.push_back (*i);
		}
	}


	std::pair<TimeAxisView*, int> const tvp = trackview_by_y_position (ypos);
	if (tvp.first == 0) {

		/* drop onto canvas background: create new tracks */

		frame = 0;

		do_import (midi_paths, Editing::ImportDistinctFiles, ImportAsTrack, SrcBest, frame);
		
		if (Profile->get_sae() || Config->get_only_copy_imported_files() || copy) {
			do_import (audio_paths, Editing::ImportDistinctFiles, Editing::ImportAsTrack, SrcBest, frame);
		} else {
			do_embed (audio_paths, Editing::ImportDistinctFiles, ImportAsTrack, frame);
		}

	} else if ((tv = dynamic_cast<RouteTimeAxisView*> (tvp.first)) != 0) {

		/* check that its a track, not a bus */

		if (tv->track()) {
			/* select the track, then embed/import */
			selection->set (tv);

			do_import (midi_paths, Editing::ImportSerializeFiles, ImportToTrack, SrcBest, frame);

			if (Profile->get_sae() || Config->get_only_copy_imported_files() || copy) {
				do_import (audio_paths, Editing::ImportSerializeFiles, Editing::ImportToTrack, SrcBest, frame);
			} else {
				do_embed (audio_paths, Editing::ImportSerializeFiles, ImportToTrack, frame);
			}
		}
	}
}

void
Editor::drop_paths (const RefPtr<Gdk::DragContext>& context,
		    int x, int y,
		    const SelectionData& data,
		    guint info, guint time)
{
	vector<string> paths;
	GdkEvent ev;
	framepos_t frame;
	double cy;

	if (convert_drop_to_paths (paths, context, x, y, data, info, time) == 0) {

		/* D-n-D coordinates are window-relative, so convert to "world" coordinates
		 */

		ev.type = GDK_BUTTON_RELEASE;
		ev.button.x = x;
		ev.button.y = y;

		frame = window_event_sample (&ev, 0, &cy);

		snap_to (frame);

		bool copy = ((context->get_actions() & (Gdk::ACTION_COPY | Gdk::ACTION_LINK | Gdk::ACTION_MOVE)) == Gdk::ACTION_COPY);
#ifdef GTKOSX
		/* We are not allowed to call recursive main event loops from within
		   the main event loop with GTK/Quartz. Since import/embed wants
		   to push up a progress dialog, defer all this till we go idle.
		*/
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun (*this, &Editor::idle_drop_paths), paths, frame, cy, copy));
#else
		drop_paths_part_two (paths, frame, cy, copy);
#endif
	}

	context->drag_finish (true, false, time);
}

/** @param allow_horiz true to allow horizontal autoscroll, otherwise false.
 *
 *  @param allow_vert true to allow vertical autoscroll, otherwise false.
 *
 */
void
Editor::maybe_autoscroll (bool allow_horiz, bool allow_vert, bool from_headers)
{
	if (!Config->get_autoscroll_editor ()) {
		return;
	}


	ArdourCanvas::Rect scrolling_boundary;
	Gtk::Allocation alloc;
	
	if (from_headers) {
		alloc = controls_layout.get_allocation ();
	} else {
		alloc = _track_canvas_viewport->get_allocation ();
		
		/* the effective width of the autoscroll boundary so
		   that we start scrolling before we hit the edge.
		   
		   this helps when the window is slammed up against the
		   right edge of the screen, making it hard to scroll
		   effectively.
		*/
		
		if (alloc.get_width() > 20) { 
			alloc.set_width (alloc.get_width() - 20);
			alloc.set_x (alloc.get_x() + 10);
		} 
	}
	
	scrolling_boundary = ArdourCanvas::Rect (alloc.get_x(), alloc.get_y(), 
						 alloc.get_x() + alloc.get_width(), 
						 alloc.get_y() + alloc.get_height());
	
	int x, y;
	Gdk::ModifierType mask;

	get_window()->get_pointer (x, y, mask);

	if ((allow_horiz && (x < scrolling_boundary.x0 || x >= scrolling_boundary.x1)) ||
	    (allow_vert && (y < scrolling_boundary.y0 || y >= scrolling_boundary.y1))) {
		start_canvas_autoscroll (allow_horiz, allow_vert, scrolling_boundary);
	}
}

bool
Editor::autoscroll_active () const
{
	return autoscroll_connection.connected ();
}

bool
Editor::autoscroll_canvas ()
{
	int x, y;
	Gdk::ModifierType mask;
	frameoffset_t dx = 0;
	bool no_stop = false;
	bool y_motion = false;

	get_window()->get_pointer (x, y, mask);

	VisualChange vc;

	if (autoscroll_horizontal_allowed) {

		framepos_t new_frame = leftmost_frame;

		/* horizontal */

		if (x > autoscroll_boundary.x1) {

			/* bring it back into view */
			dx = x - autoscroll_boundary.x1;
			dx += 10 + (2 * (autoscroll_cnt/2));

			dx = pixel_to_sample (dx);

			if (leftmost_frame < max_framepos - dx) {
				new_frame = leftmost_frame + dx;
			} else {
				new_frame = max_framepos;
			}

			no_stop = true;

		} else if (x < autoscroll_boundary.x0) {
			
			dx = autoscroll_boundary.x0 - x;
			dx += 10 + (2 * (autoscroll_cnt/2));

			dx = pixel_to_sample (dx);

			if (leftmost_frame >= dx) {
				new_frame = leftmost_frame - dx;
			} else {
				new_frame = 0;
			}

			no_stop = true;
		}
		
		if (new_frame != leftmost_frame) {
			vc.time_origin = new_frame;
			vc.add (VisualChange::TimeOrigin);
		}
	}

	if (autoscroll_vertical_allowed) {
		
		const double vertical_pos = vertical_adjustment.get_value();
		double new_pixel = vertical_pos;
		const int speed_factor = 20;

		/* vertical */ 
		
		new_pixel = vertical_pos;

		if (y < autoscroll_boundary.y0) {

			/* scroll to make higher tracks visible */

			if (autoscroll_cnt && (autoscroll_cnt % speed_factor == 0)) {
				y_motion = scroll_up_one_track ();
			}

		} else if (y > autoscroll_boundary.y1) {

			if (autoscroll_cnt && (autoscroll_cnt % speed_factor == 0)) {
				y_motion = scroll_down_one_track ();
				
			}
		}

		no_stop = true;
	}

	if (vc.pending) {

		/* change horizontal first */

		if (vc.pending) {
			visual_changer (vc);
		}

		/* now send a motion event to notify anyone who cares
		   that we have moved to a new location (because we scrolled)
		*/

		GdkEventMotion ev;

		ev.type = GDK_MOTION_NOTIFY;
		ev.state = Gdk::BUTTON1_MASK;
		
		/* the motion handler expects events in canvas coordinate space */

		/* first convert from Editor window coordinates to canvas
		 * window coordinates
		 */

		int cx;
		int cy;

		/* clamp x and y to remain within the visible area */

		x = min (max ((ArdourCanvas::Coord) x, autoscroll_boundary.x0), autoscroll_boundary.x1);
		y = min (max ((ArdourCanvas::Coord) y, autoscroll_boundary.y0), autoscroll_boundary.y1);

		translate_coordinates (*_track_canvas_viewport, x, y, cx, cy);

		ArdourCanvas::Duple d = _track_canvas->window_to_canvas (ArdourCanvas::Duple (cx, cy));
		ev.x = d.x;
		ev.y = d.y;

		motion_handler (0, (GdkEvent*) &ev, true);

	} else if (no_stop) {

		/* not changing visual state but pointer is outside the scrolling boundary
		 * so we still need to deliver a fake motion event 
		 */

		GdkEventMotion ev;

		ev.type = GDK_MOTION_NOTIFY;
		ev.state = Gdk::BUTTON1_MASK;
		
		/* the motion handler expects events in canvas coordinate space */

		/* first convert from Editor window coordinates to canvas
		 * window coordinates
		 */

		int cx;
		int cy;

		/* clamp x and y to remain within the visible area. except
		 * .. if horizontal scrolling is allowed, always allow us to
		 * move back to zero
		 */

		if (autoscroll_horizontal_allowed) {
			x = min (max ((ArdourCanvas::Coord) x, 0.0), autoscroll_boundary.x1);
		} else {
			x = min (max ((ArdourCanvas::Coord) x, autoscroll_boundary.x0), autoscroll_boundary.x1);
		}
		y = min (max ((ArdourCanvas::Coord) y, autoscroll_boundary.y0), autoscroll_boundary.y1);

		translate_coordinates (*_track_canvas_viewport, x, y, cx, cy);

		ArdourCanvas::Duple d = _track_canvas->window_to_canvas (ArdourCanvas::Duple (cx, cy));
		ev.x = d.x;
		ev.y = d.y;

		motion_handler (0, (GdkEvent*) &ev, true);

	} else {
		stop_canvas_autoscroll ();
		return false;
	}

	autoscroll_cnt++;

	return true; /* call me again */
}	

void
Editor::start_canvas_autoscroll (bool allow_horiz, bool allow_vert, const ArdourCanvas::Rect& boundary)
{
	if (!_session) {
		return;
	}

	stop_canvas_autoscroll ();

	autoscroll_cnt = 0;
	autoscroll_horizontal_allowed = allow_horiz;
	autoscroll_vertical_allowed = allow_vert;
	autoscroll_boundary = boundary;

	/* do the first scroll right now
	*/

	autoscroll_canvas ();

	/* scroll again at very very roughly 30FPS */

	autoscroll_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &Editor::autoscroll_canvas), 30);
}

void
Editor::stop_canvas_autoscroll ()
{
	autoscroll_connection.disconnect ();
}

bool
Editor::left_track_canvas (GdkEventCrossing */*ev*/)
{
	DropDownKeys ();
	within_track_canvas = false;
	set_entered_track (0);
	set_entered_regionview (0);
	reset_canvas_action_sensitivity (false);
	return false;
}

bool
Editor::entered_track_canvas (GdkEventCrossing */*ev*/)
{
	within_track_canvas = true;
	reset_canvas_action_sensitivity (true);
	return FALSE;
}

void
Editor::_ensure_time_axis_view_is_visible (const TimeAxisView& tav, bool at_top)
{
	double begin = tav.y_position();
	double v = vertical_adjustment.get_value ();

	if (!at_top && (begin < v || begin + tav.current_height() > v + _visible_canvas_height)) {
		/* try to put the TimeAxisView roughly central */
		if (begin >= _visible_canvas_height/2.0) {
			begin -= _visible_canvas_height/2.0;
		}
	}

	/* Clamp the y pos so that we do not extend beyond the canvas full
	 * height. 
	 */
	if (_full_canvas_height - begin < _visible_canvas_height){
		begin = _full_canvas_height - _visible_canvas_height;
	}

	vertical_adjustment.set_value (begin);
}

/** Called when the main vertical_adjustment has changed */
void
Editor::tie_vertical_scrolling ()
{
	if (pending_visual_change.idle_handler_id < 0) {
		_summary->set_overlays_dirty ();
	}
}

void
Editor::set_horizontal_position (double p)
{
	horizontal_adjustment.set_value (p);

	leftmost_frame = (framepos_t) floor (p * samples_per_pixel);

	update_fixed_rulers ();
	redisplay_tempo (true);

	if (pending_visual_change.idle_handler_id < 0) {
		_summary->set_overlays_dirty ();
	}

	update_video_timeline();
}

void
Editor::color_handler()
{
	playhead_cursor->set_color (ARDOUR_UI::config()->get_canvasvar_PlayHead());
	_verbose_cursor->set_color (ARDOUR_UI::config()->get_canvasvar_VerboseCanvasCursor());

	meter_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_MeterBar());
	meter_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	tempo_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TempoBar());
	tempo_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	marker_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_MarkerBar());
	marker_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	cd_marker_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_CDMarkerBar());
	cd_marker_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	range_marker_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_RangeMarkerBar());
	range_marker_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	transport_marker_bar->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TransportMarkerBar());
	transport_marker_bar->set_outline_color (ARDOUR_UI::config()->get_canvasvar_MarkerBarSeparator());

	cd_marker_bar_drag_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_RangeDragBarRect());
	cd_marker_bar_drag_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_RangeDragBarRect());

	range_bar_drag_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_RangeDragBarRect());
	range_bar_drag_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_RangeDragBarRect());

	transport_bar_drag_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TransportDragRect());
	transport_bar_drag_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TransportDragRect());

	transport_loop_range_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TransportLoopRect());
	transport_loop_range_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TransportLoopRect());

	transport_punch_range_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TransportPunchRect());
	transport_punch_range_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TransportPunchRect());

	transport_punchin_line->set_outline_color (ARDOUR_UI::config()->get_canvasvar_PunchLine());
	transport_punchout_line->set_outline_color (ARDOUR_UI::config()->get_canvasvar_PunchLine());

	zoom_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_ZoomRect());
	zoom_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_ZoomRect());

	rubberband_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_RubberBandRect());
	rubberband_rect->set_fill_color ((guint32) ARDOUR_UI::config()->get_canvasvar_RubberBandRect());

	location_marker_color = ARDOUR_UI::config()->get_canvasvar_LocationMarker();
	location_range_color = ARDOUR_UI::config()->get_canvasvar_LocationRange();
	location_cd_marker_color = ARDOUR_UI::config()->get_canvasvar_LocationCDMarker();
	location_loop_color = ARDOUR_UI::config()->get_canvasvar_LocationLoop();
	location_punch_color = ARDOUR_UI::config()->get_canvasvar_LocationPunch();

	refresh_location_display ();
/*
	redisplay_tempo (true);

	if (_session)
	      _session->tempo_map().apply_with_metrics (*this, &Editor::draw_metric_marks); // redraw metric markers
*/
}

double
Editor::horizontal_position () const
{
	return sample_to_pixel (leftmost_frame);
}

void
Editor::set_canvas_cursor (Gdk::Cursor* cursor, bool save)
{
	if (save) {
		current_canvas_cursor = cursor;
	}

	Glib::RefPtr<Gdk::Window> win = _track_canvas->get_window();

	if (win) {
	        _track_canvas->get_window()->set_cursor (*cursor);
	}
}

bool
Editor::track_canvas_key_press (GdkEventKey*)
{
	/* XXX: event does not report the modifier key pressed down, AFAICS, so use the Keyboard object instead */
	if (mouse_mode == Editing::MouseZoom && Keyboard::the_keyboard().key_is_down (GDK_Control_L)) {
		set_canvas_cursor (_cursors->zoom_out, true);
	}

	return false;
}

bool
Editor::track_canvas_key_release (GdkEventKey*)
{
	if (mouse_mode == Editing::MouseZoom && !Keyboard::the_keyboard().key_is_down (GDK_Control_L)) {
		set_canvas_cursor (_cursors->zoom_in, true);
	}

	return false;
}

double
Editor::clamp_verbose_cursor_x (double x)
{
	if (x < 0) {
		x = 0;
	} else {
		x = min (_visible_canvas_width - 200.0, x);
	}
	return x;
}

double
Editor::clamp_verbose_cursor_y (double y)
{
	y = max (0.0, y);
	y = min (_visible_canvas_height - 50, y);
	return y;
}

ArdourCanvas::Group*
Editor::get_time_bars_group () const
{
	return _time_bars_canvas->root();
}

ArdourCanvas::Group*
Editor::get_track_canvas_group() const
{
	return _track_canvas->root();
}

ArdourCanvas::GtkCanvasViewport*
Editor::get_time_bars_canvas() const
{
	return _time_bars_canvas_viewport;
}

ArdourCanvas::GtkCanvasViewport*
Editor::get_track_canvas() const
{
	return _track_canvas_viewport;
}
