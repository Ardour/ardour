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

#include <libgnomecanvasmm/init.h>
#include <libgnomecanvasmm/pixbuf.h>
#include <jack/types.h>

#include "gtkmm2ext/utils.h"

#include "ardour/profile.h"
#include "ardour/rc_configuration.h"

#include "ardour_ui.h"
#include "editor.h"
#include "global_signals.h"
#include "waveview.h"
#include "simplerect.h"
#include "simpleline.h"
#include "imageframe.h"
#include "waveview_p.h"
#include "simplerect_p.h"
#include "simpleline_p.h"
#include "imageframe_p.h"
#include "canvas_impl.h"
#include "canvas-noevent-text.h"
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

const double max_canvas_coordinate = (double) JACK_MAX_FRAMES;

extern "C"
{

GType gnome_canvas_simpleline_get_type(void);
GType gnome_canvas_simplerect_get_type(void);
GType gnome_canvas_waveview_get_type(void);
GType gnome_canvas_imageframe_get_type(void);

}

static void ardour_canvas_type_init()
{
	// Map gtypes to gtkmm wrapper-creation functions:

	Glib::wrap_register(gnome_canvas_simpleline_get_type(), &Gnome::Canvas::SimpleLine_Class::wrap_new);
	Glib::wrap_register(gnome_canvas_simplerect_get_type(), &Gnome::Canvas::SimpleRect_Class::wrap_new);
	Glib::wrap_register(gnome_canvas_waveview_get_type(), &Gnome::Canvas::WaveView_Class::wrap_new);
	// Glib::wrap_register(gnome_canvas_imageframe_get_type(), &Gnome::Canvas::ImageFrame_Class::wrap_new);

	// Register the gtkmm gtypes:

	(void) Gnome::Canvas::WaveView::get_type();
	(void) Gnome::Canvas::SimpleLine::get_type();
	(void) Gnome::Canvas::SimpleRect::get_type();
	(void) Gnome::Canvas::ImageFrame::get_type();
}

void
Editor::initialize_canvas ()
{
	if (getenv ("ARDOUR_NON_AA_CANVAS")) {
		track_canvas = new ArdourCanvas::Canvas ();
	} else {
		track_canvas = new ArdourCanvas::CanvasAA ();
	}

	track_canvas->set_can_default (true);
	set_default (*track_canvas);

	ArdourCanvas::init ();
	ardour_canvas_type_init ();

	/* don't try to center the canvas */

	track_canvas->set_center_scroll_region (false);
	track_canvas->set_dither (Gdk::RGB_DITHER_NONE);

        gint phys_width = physical_screen_width (Glib::RefPtr<Gdk::Window>());
        gint phys_height = physical_screen_height (Glib::RefPtr<Gdk::Window>());

	_verbose_cursor = new VerboseCursor (this);

	/* on the bottom, an image */

	if (Profile->get_sae()) {
		Image img (::get_icon (X_("saelogo")));
		logo_item = new ArdourCanvas::Pixbuf (*track_canvas->root(), 0.0, 0.0, img.get_pixbuf());
		// logo_item->property_height_in_pixels() = true;
		// logo_item->property_width_in_pixels() = true;
		// logo_item->property_height_set() = true;
		// logo_item->property_width_set() = true;
		logo_item->show ();
	}

	/* a group to hold time (measure) lines */
	time_line_group = new ArdourCanvas::Group (*track_canvas->root());

#ifdef GTKOSX
	/*XXX please don't laugh. this actually improves canvas performance on osx */
	bogus_background_rect =  new ArdourCanvas::SimpleRect (*time_line_group, 0.0, 0.0, max_canvas_coordinate/3, phys_height);
	bogus_background_rect->property_outline_pixels() = 0;
#endif
	transport_loop_range_rect = new ArdourCanvas::SimpleRect (*time_line_group, 0.0, 0.0, 0.0, phys_height);
	transport_loop_range_rect->property_outline_pixels() = 1;
	transport_loop_range_rect->hide();

	transport_punch_range_rect = new ArdourCanvas::SimpleRect (*time_line_group, 0.0, 0.0, 0.0, phys_height);
	transport_punch_range_rect->property_outline_pixels() = 0;
	transport_punch_range_rect->hide();

	_background_group = new ArdourCanvas::Group (*track_canvas->root());
	_master_group = new ArdourCanvas::Group (*track_canvas->root());

	_trackview_group = new ArdourCanvas::Group (*_master_group);
	_region_motion_group = new ArdourCanvas::Group (*_trackview_group);

	meter_bar_group = new ArdourCanvas::Group (*track_canvas->root ());
	meter_bar = new ArdourCanvas::SimpleRect (*meter_bar_group, 0.0, 0.0, phys_width, timebar_height - 1);
	meter_bar->property_outline_pixels() = 1;
	meter_bar->property_outline_what() = 0x8;

	tempo_bar_group = new ArdourCanvas::Group (*track_canvas->root ());
	tempo_bar = new ArdourCanvas::SimpleRect (*tempo_bar_group, 0.0, 0.0, phys_width, (timebar_height - 1));
	tempo_bar->property_outline_pixels() = 1;
	tempo_bar->property_outline_what() = 0x8;

	range_marker_bar_group = new ArdourCanvas::Group (*track_canvas->root ());
	range_marker_bar = new ArdourCanvas::SimpleRect (*range_marker_bar_group, 0.0, 0.0, phys_width, (timebar_height - 1));
	range_marker_bar->property_outline_pixels() = 1;
	range_marker_bar->property_outline_what() = 0x8;

	transport_marker_bar_group = new ArdourCanvas::Group (*track_canvas->root ());
	transport_marker_bar = new ArdourCanvas::SimpleRect (*transport_marker_bar_group, 0.0, 0.0,  phys_width, (timebar_height - 1));
	transport_marker_bar->property_outline_pixels() = 1;
	transport_marker_bar->property_outline_what() = 0x8;

	marker_bar_group = new ArdourCanvas::Group (*track_canvas->root ());
	marker_bar = new ArdourCanvas::SimpleRect (*marker_bar_group, 0.0, 0.0, phys_width, (timebar_height - 1));
	marker_bar->property_outline_pixels() = 1;
	marker_bar->property_outline_what() = 0x8;

	cd_marker_bar_group = new ArdourCanvas::Group (*track_canvas->root ());
	cd_marker_bar = new ArdourCanvas::SimpleRect (*cd_marker_bar_group, 0.0, 0.0, phys_width, (timebar_height - 1));
	cd_marker_bar->property_outline_pixels() = 1;
 	cd_marker_bar->property_outline_what() = 0x8;

#ifdef WITH_VIDEOTIMELINE
	videotl_bar_group = new ArdourCanvas::Group (*track_canvas->root ());
	if (Profile->get_sae()) {
		videotl_bar = new ArdourCanvas::SimpleRect (*videotl_bar_group, 0.0, 0.0, phys_width, (timebar_height * videotl_bar_height - 1));
		videotl_bar->property_outline_pixels() = 1;
	} else {
		videotl_bar = new ArdourCanvas::SimpleRect (*videotl_bar_group, 0.0, 0.0, phys_width, (timebar_height * videotl_bar_height));
		videotl_bar->property_outline_pixels() = 0;
	}
	videotl_bar->property_outline_what() = (0x1 | 0x8);
	ARDOUR_UI::instance()->video_timeline = new VideoTimeLine(this, videotl_bar_group, (timebar_height * videotl_bar_height));
#endif

	timebar_group =  new ArdourCanvas::Group (*track_canvas->root(), 0.0, 0.0);
	cursor_group = new ArdourCanvas::Group (*track_canvas->root(), 0.0, 0.0);

	meter_group = new ArdourCanvas::Group (*timebar_group, 0.0, timebar_height * 5.0);
	tempo_group = new ArdourCanvas::Group (*timebar_group, 0.0, timebar_height * 4.0);
	range_marker_group = new ArdourCanvas::Group (*timebar_group, 0.0, timebar_height * 3.0);
	transport_marker_group = new ArdourCanvas::Group (*timebar_group, 0.0, timebar_height * 2.0);
	marker_group = new ArdourCanvas::Group (*timebar_group, 0.0, timebar_height);
	cd_marker_group = new ArdourCanvas::Group (*timebar_group, 0.0, 0.0);
#ifdef WITH_VIDEOTIMELINE
	videotl_group = new ArdourCanvas::Group (*timebar_group, 0.0, 0.0);
#endif

	cd_marker_bar_drag_rect = new ArdourCanvas::SimpleRect (*cd_marker_group, 0.0, 0.0, 100, timebar_height);
	cd_marker_bar_drag_rect->property_outline_pixels() = 0;
	cd_marker_bar_drag_rect->hide ();

	range_bar_drag_rect = new ArdourCanvas::SimpleRect (*range_marker_group, 0.0, 0.0, 100, timebar_height);
	range_bar_drag_rect->property_outline_pixels() = 0;
	range_bar_drag_rect->hide ();

	transport_bar_drag_rect = new ArdourCanvas::SimpleRect (*transport_marker_group, 0.0, 0.0, 100, timebar_height);
	transport_bar_drag_rect->property_outline_pixels() = 0;
	transport_bar_drag_rect->hide ();

	transport_punchin_line = new ArdourCanvas::SimpleLine (*_master_group);
	transport_punchin_line->property_x1() = 0.0;
	transport_punchin_line->property_y1() = 0.0;
	transport_punchin_line->property_x2() = 0.0;
	transport_punchin_line->property_y2() = phys_height;
	transport_punchin_line->hide ();

	transport_punchout_line  = new ArdourCanvas::SimpleLine (*_master_group);
	transport_punchout_line->property_x1() = 0.0;
	transport_punchout_line->property_y1() = 0.0;
	transport_punchout_line->property_x2() = 0.0;
	transport_punchout_line->property_y2() = phys_height;
	transport_punchout_line->hide();

	// used to show zoom mode active zooming
	zoom_rect = new ArdourCanvas::SimpleRect (*_master_group, 0.0, 0.0, 0.0, 0.0);
	zoom_rect->property_outline_pixels() = 1;
	zoom_rect->hide();

	zoom_rect->signal_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_zoom_rect_event), (ArdourCanvas::Item*) 0));

	// used as rubberband rect
	rubberband_rect = new ArdourCanvas::SimpleRect (*_trackview_group, 0.0, 0.0, 0.0, 0.0);

	rubberband_rect->property_outline_pixels() = 1;
	rubberband_rect->hide();

	tempo_bar->signal_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_tempo_bar_event), tempo_bar));
	meter_bar->signal_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_meter_bar_event), meter_bar));
	marker_bar->signal_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_marker_bar_event), marker_bar));
	cd_marker_bar->signal_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_cd_marker_bar_event), cd_marker_bar));
#ifdef WITH_VIDEOTIMELINE
	videotl_bar_group->signal_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_videotl_bar_event), videotl_bar));
	//videotl_bar->signal_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_videotl_bar_event), videotl_bar));
#endif
	range_marker_bar->signal_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_range_marker_bar_event), range_marker_bar));
	transport_marker_bar->signal_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_transport_marker_bar_event), transport_marker_bar));

	playhead_cursor = new EditorCursor (*this, &Editor::canvas_playhead_cursor_event);

	if (logo_item) {
		logo_item->lower_to_bottom ();
	}
	/* need to handle 4 specific types of events as catch-alls */

	track_canvas->signal_scroll_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_scroll_event));
	track_canvas->signal_motion_notify_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_motion_notify_event));
	track_canvas->signal_button_press_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_button_press_event));
	track_canvas->signal_button_release_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_button_release_event));
	track_canvas->signal_drag_motion().connect (sigc::mem_fun (*this, &Editor::track_canvas_drag_motion));
	track_canvas->signal_key_press_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_key_press));
	track_canvas->signal_key_release_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_key_release));

	track_canvas->set_name ("EditorMainCanvas");
	track_canvas->add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	track_canvas->signal_leave_notify_event().connect (sigc::mem_fun(*this, &Editor::left_track_canvas), false);
	track_canvas->signal_enter_notify_event().connect (sigc::mem_fun(*this, &Editor::entered_track_canvas), false);
	track_canvas->set_flags (CAN_FOCUS);

	/* set up drag-n-drop */

	vector<TargetEntry> target_table;

	// Drag-N-Drop from the region list can generate this target
	target_table.push_back (TargetEntry ("regions"));

	target_table.push_back (TargetEntry ("text/plain"));
	target_table.push_back (TargetEntry ("text/uri-list"));
	target_table.push_back (TargetEntry ("application/x-rootwin-drop"));

	track_canvas->drag_dest_set (target_table);
	track_canvas->signal_drag_data_received().connect (sigc::mem_fun(*this, &Editor::track_canvas_drag_data_received));

	track_canvas->signal_size_allocate().connect (sigc::mem_fun(*this, &Editor::track_canvas_allocate));

	ColorsChanged.connect (sigc::mem_fun (*this, &Editor::color_handler));
	color_handler();

}

void
Editor::track_canvas_allocate (Gtk::Allocation alloc)
{
	canvas_allocation = alloc;
	track_canvas_size_allocated ();
}

bool
Editor::track_canvas_size_allocated ()
{
	bool height_changed = _canvas_height != canvas_allocation.get_height();

	_canvas_width = canvas_allocation.get_width();
	_canvas_height = canvas_allocation.get_height();

	if (_session) {
		TrackViewList::iterator i;

		for (i = track_views.begin(); i != track_views.end(); ++i) {
			(*i)->clip_to_viewport ();
		}
	}

	if (height_changed) {
		if (playhead_cursor) {
			playhead_cursor->set_length (_canvas_height);
		}

		for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
			i->second->canvas_height_set (_canvas_height);
		}

		vertical_adjustment.set_page_size (_canvas_height);
		last_trackview_group_vertical_offset = get_trackview_group_vertical_offset ();
		if ((vertical_adjustment.get_value() + _canvas_height) >= vertical_adjustment.get_upper()) {
			/*
			   We're increasing the size of the canvas while the bottom is visible.
			   We scroll down to keep in step with the controls layout.
			*/
			vertical_adjustment.set_value (full_canvas_height - _canvas_height);
		}
	}

	update_fixed_rulers();
	redisplay_tempo (false);
	_summary->set_overlays_dirty ();

	return false;
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
Editor::idle_drop_paths (vector<string> paths, framepos_t frame, double ypos)
{
	drop_paths_part_two (paths, frame, ypos);
	return false;
}

void
Editor::drop_paths_part_two (const vector<string>& paths, framepos_t frame, double ypos)
{
	RouteTimeAxisView* tv;

	std::pair<TimeAxisView*, int> const tvp = trackview_by_y_position (ypos);
	if (tvp.first == 0) {

		/* drop onto canvas background: create new tracks */

		frame = 0;

		if (Profile->get_sae() || Config->get_only_copy_imported_files()) {
			do_import (paths, Editing::ImportDistinctFiles, Editing::ImportAsTrack, SrcBest, frame);
		} else {
			do_embed (paths, Editing::ImportDistinctFiles, ImportAsTrack, frame);
		}

	} else if ((tv = dynamic_cast<RouteTimeAxisView*> (tvp.first)) != 0) {

		/* check that its an audio track, not a bus */

		if (tv->track()) {
			/* select the track, then embed/import */
			selection->set (tv);

			if (Profile->get_sae() || Config->get_only_copy_imported_files()) {
				do_import (paths, Editing::ImportSerializeFiles, Editing::ImportToTrack, SrcBest, frame);
			} else {
				do_embed (paths, Editing::ImportSerializeFiles, ImportToTrack, frame);
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
	double wx;
	double wy;
	double cy;

	if (convert_drop_to_paths (paths, context, x, y, data, info, time) == 0) {

		/* D-n-D coordinates are window-relative, so convert to "world" coordinates
		 */

		track_canvas->window_to_world (x, y, wx, wy);

		ev.type = GDK_BUTTON_RELEASE;
		ev.button.x = wx;
		ev.button.y = wy;

		frame = event_frame (&ev, 0, &cy);

		snap_to (frame);

#ifdef GTKOSX
		/* We are not allowed to call recursive main event loops from within
		   the main event loop with GTK/Quartz. Since import/embed wants
		   to push up a progress dialog, defer all this till we go idle.
		*/
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun (*this, &Editor::idle_drop_paths), paths, frame, cy));
#else
		drop_paths_part_two (paths, frame, cy);
#endif
	}

	context->drag_finish (true, false, time);
}

/** If the editor window is arranged such that the edge of the trackview is right up
 *  against the edge of the screen, autoscroll will not work very well.  In this situation,
 *  we start autoscrolling some distance in from the right-hand-side of the screen edge;
 *  this is the distance at which that happens.
 */
int
Editor::autoscroll_fudge_threshold () const
{
	return current_page_frames() / 6;
}

/** @param allow_horiz true to allow horizontal autoscroll, otherwise false.
 *  @param allow_vert true to allow vertical autoscroll, otherwise false.
 *  @param moving_left true if we are moving left, so we only want to autoscroll on the left of the canvas,
 *  otherwise false, so we only want to autoscroll on the right of the canvas.
 *  @param moving_up true if we are moving up, so we only want to autoscroll at the top of the canvas,
 *  otherwise false, so we only want to autoscroll at the bottom of the canvas.
 */
void
Editor::maybe_autoscroll (bool allow_horiz, bool allow_vert, bool moving_left, bool moving_up)
{
	if (!Config->get_autoscroll_editor ()) {
		return;
	}
	
	bool startit = false;

	/* Work out the distance between the right hand edge of the trackview and the edge of
	   the monitor that it is on.
	*/

	Glib::RefPtr<Gdk::Window> gdk_window = get_window ();
	Gdk::Rectangle window_rect;
	gdk_window->get_frame_extents (window_rect);
	
	Glib::RefPtr<Gdk::Screen> screen = get_screen ();
	Gdk::Rectangle root_rect;
	screen->get_root_window()->get_frame_extents (root_rect);

	Gtk::Allocation editor_list = _the_notebook.get_allocation ();

	framecnt_t distance = pixel_to_frame (root_rect.get_x() + root_rect.get_width() - window_rect.get_x() - window_rect.get_width());
	if (_the_notebook.is_visible ()) {
		distance += pixel_to_frame (editor_list.get_width());
	}

	/* Note whether we're fudging the autoscroll (see autoscroll_fudge_threshold) */
	_autoscroll_fudging = (distance < autoscroll_fudge_threshold ());

	double const ty = _drags->current_pointer_y() - get_trackview_group_vertical_offset ();

	autoscroll_y = 0;
	autoscroll_x = 0;
	if (ty < canvas_timebars_vsize && moving_up && allow_vert) {
		autoscroll_y = -1;
		startit = true;
	} else if (ty > _canvas_height && !moving_up && allow_vert) {
		autoscroll_y = 1;
		startit = true;
	}

	framepos_t rightmost_frame = leftmost_frame + current_page_frames();
	if (_autoscroll_fudging) {
		rightmost_frame -= autoscroll_fudge_threshold ();
	}

	if (_drags->current_pointer_frame() > rightmost_frame && allow_horiz) {
		if (rightmost_frame < max_framepos && !moving_left) {
			autoscroll_x = 1;
			startit = true;
		}
	} else if (_drags->current_pointer_frame() < leftmost_frame && allow_horiz) {
		if (leftmost_frame > 0 && moving_left) {
			autoscroll_x = -1;
			startit = true;
		}
	}

	if (autoscroll_active && ((autoscroll_x != last_autoscroll_x) || (autoscroll_y != last_autoscroll_y) || (autoscroll_x == 0 && autoscroll_y == 0))) {
		stop_canvas_autoscroll ();
	}

	if (startit && autoscroll_timeout_tag < 0) {
		start_canvas_autoscroll (autoscroll_x, autoscroll_y);
	}

	last_autoscroll_x = autoscroll_x;
	last_autoscroll_y = autoscroll_y;
}

gint
Editor::_autoscroll_canvas (void *arg)
{
        return ((Editor *) arg)->autoscroll_canvas ();
}

bool
Editor::autoscroll_canvas ()
{
	framepos_t new_frame;
	framepos_t limit = max_framepos - current_page_frames();
	GdkEventMotion ev;
	double new_pixel;
	double target_pixel;
	
	if (autoscroll_x_distance != 0) {

		if (autoscroll_x > 0) {
			autoscroll_x_distance = (_drags->current_pointer_frame() - (leftmost_frame + current_page_frames())) / 3;
			if (_autoscroll_fudging) {
				autoscroll_x_distance += autoscroll_fudge_threshold () / 3;
			}
		} else if (autoscroll_x < 0) {
			autoscroll_x_distance = (leftmost_frame - _drags->current_pointer_frame()) / 3;

		}
	}

	if (autoscroll_y_distance != 0) {
		if (autoscroll_y > 0) {
			autoscroll_y_distance = (_drags->current_pointer_y() - (get_trackview_group_vertical_offset() + _canvas_height)) / 3;
		} else if (autoscroll_y < 0) {

			autoscroll_y_distance = (vertical_adjustment.get_value () - _drags->current_pointer_y()) / 3;
		}
	}

	if (autoscroll_x < 0) {
		if (leftmost_frame < autoscroll_x_distance) {
			new_frame = 0;
		} else {
			new_frame = leftmost_frame - autoscroll_x_distance;
		}
 	} else if (autoscroll_x > 0) {
		if (leftmost_frame > limit - autoscroll_x_distance) {
			new_frame = limit;
		} else {
			new_frame = leftmost_frame + autoscroll_x_distance;
		}
	} else {
		new_frame = leftmost_frame;
	}

	double vertical_pos = vertical_adjustment.get_value();

	if (autoscroll_y < 0) {

		if (vertical_pos < autoscroll_y_distance) {
			new_pixel = 0;
		} else {
			new_pixel = vertical_pos - autoscroll_y_distance;
		}

		target_pixel = _drags->current_pointer_y() - autoscroll_y_distance;
		target_pixel = max (target_pixel, 0.0);

 	} else if (autoscroll_y > 0) {

		double top_of_bottom_of_canvas = full_canvas_height - _canvas_height;

		if (vertical_pos > full_canvas_height - autoscroll_y_distance) {
			new_pixel = full_canvas_height;
		} else {
			new_pixel = vertical_pos + autoscroll_y_distance;
		}

		new_pixel = min (top_of_bottom_of_canvas, new_pixel);

		target_pixel = _drags->current_pointer_y() + autoscroll_y_distance;

		/* don't move to the full canvas height because the item will be invisible
		   (its top edge will line up with the bottom of the visible canvas.
		*/

		target_pixel = min (target_pixel, full_canvas_height - 10);

	} else {
	  	target_pixel = _drags->current_pointer_y();
		new_pixel = vertical_pos;
	}

	if ((new_frame == 0 || new_frame == limit) && (new_pixel == 0 || new_pixel == DBL_MAX)) {
		/* we are done */
		return false;
	}

	if (new_frame != leftmost_frame) {
		reset_x_origin (new_frame);
	}

	vertical_adjustment.set_value (new_pixel);

	/* fake an event. */

	Glib::RefPtr<Gdk::Window> canvas_window = const_cast<Editor*>(this)->track_canvas->get_window();
	gint x, y;
	Gdk::ModifierType mask;
	canvas_window->get_pointer (x, y, mask);
	ev.type = GDK_MOTION_NOTIFY;
	ev.state = Gdk::BUTTON1_MASK;
	ev.x = x;
	ev.y = y;

	motion_handler (0, (GdkEvent*) &ev, true);

	autoscroll_cnt++;

	if (autoscroll_cnt == 1) {

		/* connect the timeout so that we get called repeatedly */

		autoscroll_timeout_tag = g_idle_add ( _autoscroll_canvas, this);
		return false;

	}

	return true;
}

void
Editor::start_canvas_autoscroll (int dx, int dy)
{
	if (!_session || autoscroll_active) {
		return;
	}

	stop_canvas_autoscroll ();

	autoscroll_active = true;
	autoscroll_x = dx;
	autoscroll_y = dy;
	autoscroll_x_distance = (framepos_t) floor (current_page_frames()/50.0);
	autoscroll_y_distance = fabs (dy * 5); /* pixels */
	autoscroll_cnt = 0;

	/* do it right now, which will start the repeated callbacks */

	autoscroll_canvas ();
}

void
Editor::stop_canvas_autoscroll ()
{
	if (autoscroll_timeout_tag >= 0) {
		g_source_remove (autoscroll_timeout_tag);
		autoscroll_timeout_tag = -1;
	}

	autoscroll_active = false;
}

bool
Editor::left_track_canvas (GdkEventCrossing */*ev*/)
{
	DropDownKeys ();
	within_track_canvas = false;
	//cerr << "left track canvas\n";
	set_entered_track (0);
	set_entered_regionview (0);
	reset_canvas_action_sensitivity (false);
	return false;
}

bool
Editor::entered_track_canvas (GdkEventCrossing */*ev*/)
{
	//cerr << "entered track canvas\n";
	within_track_canvas = true;
	reset_canvas_action_sensitivity (true);
	return FALSE;
}

void
Editor::ensure_time_axis_view_is_visible (const TimeAxisView& tav)
{
	double begin = tav.y_position();

	double v = vertical_adjustment.get_value ();

	if (begin < v || begin + tav.current_height() > v + _canvas_height - canvas_timebars_vsize) {
		/* try to put the TimeAxisView roughly central */
		if (begin >= _canvas_height/2.0) {
			begin -= _canvas_height/2.0;
		}
		vertical_adjustment.set_value (begin);
	}
}

void
Editor::tie_vertical_scrolling ()
{
	scroll_canvas_vertically ();

	/* this will do an immediate redraw */

	controls_layout.get_vadjustment()->set_value (vertical_adjustment.get_value());

	if (pending_visual_change.idle_handler_id < 0) {
		_summary->set_overlays_dirty ();
	}
}

void
Editor::set_horizontal_position (double p)
{
	/* horizontal scrolling only */
	double x1, y1, x2, y2, x_delta;
	_master_group->get_bounds (x1, y1, x2, y2);

	x_delta = - (x1 + p);

	_master_group->move (x_delta, 0);
	timebar_group->move (x_delta, 0);
	time_line_group->move (x_delta, 0);
	cursor_group->move (x_delta, 0);

	leftmost_frame = (framepos_t) floor (p * frames_per_unit);

	update_fixed_rulers ();
	redisplay_tempo (true);

	if (pending_visual_change.idle_handler_id < 0) {
		_summary->set_overlays_dirty ();
	}

#ifdef WITH_VIDEOTIMELINE
	update_video_timeline();
#endif

	HorizontalPositionChanged (); /* EMIT SIGNAL */

#ifndef GTKOSX
	if (!autoscroll_active && !_stationary_playhead) {
		/* force rulers and canvas to move in lock step */
		while (gtk_events_pending ()) {
			gtk_main_iteration ();
		}
	}
#endif
}

void
Editor::scroll_canvas_vertically ()
{
	/* vertical scrolling only */

	double y_delta;

	y_delta = last_trackview_group_vertical_offset - get_trackview_group_vertical_offset ();
	_trackview_group->move (0, y_delta);
	_background_group->move (0, y_delta);

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->clip_to_viewport ();
	}
	last_trackview_group_vertical_offset = get_trackview_group_vertical_offset ();
	/* required to keep the controls_layout in lock step with the canvas group */
	update_canvas_now ();
}

void
Editor::color_handler()
{
	playhead_cursor->canvas_item.property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_PlayHead.get();
	_verbose_cursor->set_color (ARDOUR_UI::config()->canvasvar_VerboseCanvasCursor.get());

	meter_bar->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MeterBar.get();
	meter_bar->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerBarSeparator.get();

	tempo_bar->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_TempoBar.get();
	tempo_bar->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerBarSeparator.get();

	marker_bar->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerBar.get();
	marker_bar->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerBarSeparator.get();

	cd_marker_bar->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CDMarkerBar.get();
	cd_marker_bar->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerBarSeparator.get();

#ifdef WITH_VIDEOTIMELINE
	videotl_bar->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_VideoBar.get();
	videotl_bar->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerBarSeparator.get();
#endif

	range_marker_bar->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_RangeMarkerBar.get();
	range_marker_bar->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerBarSeparator.get();

	transport_marker_bar->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_TransportMarkerBar.get();
	transport_marker_bar->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerBarSeparator.get();

	cd_marker_bar_drag_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_RangeDragBarRect.get();
	cd_marker_bar_drag_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_RangeDragBarRect.get();

	range_bar_drag_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_RangeDragBarRect.get();
	range_bar_drag_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_RangeDragBarRect.get();

	transport_bar_drag_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_TransportDragRect.get();
	transport_bar_drag_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_TransportDragRect.get();

	transport_loop_range_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_TransportLoopRect.get();
	transport_loop_range_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_TransportLoopRect.get();

	transport_punch_range_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_TransportPunchRect.get();
	transport_punch_range_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_TransportPunchRect.get();

	transport_punchin_line->property_color_rgba() = ARDOUR_UI::config()->canvasvar_PunchLine.get();
	transport_punchout_line->property_color_rgba() = ARDOUR_UI::config()->canvasvar_PunchLine.get();

	zoom_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_ZoomRect.get();
	zoom_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_ZoomRect.get();

	rubberband_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_RubberBandRect.get();
	rubberband_rect->property_fill_color_rgba() = (guint32) ARDOUR_UI::config()->canvasvar_RubberBandRect.get();

	location_marker_color = ARDOUR_UI::config()->canvasvar_LocationMarker.get();
	location_range_color = ARDOUR_UI::config()->canvasvar_LocationRange.get();
	location_cd_marker_color = ARDOUR_UI::config()->canvasvar_LocationCDMarker.get();
	location_loop_color = ARDOUR_UI::config()->canvasvar_LocationLoop.get();
	location_punch_color = ARDOUR_UI::config()->canvasvar_LocationPunch.get();

	refresh_location_display ();
/*
	redisplay_tempo (true);

	if (_session)
	      _session->tempo_map().apply_with_metrics (*this, &Editor::draw_metric_marks); // redraw metric markers
*/
}

void
Editor::flush_canvas ()
{
	if (is_mapped()) {
		update_canvas_now ();
		// gdk_window_process_updates (GTK_LAYOUT(track_canvas->gobj())->bin_window, true);
	}
}

void
Editor::update_canvas_now ()
{
	/* GnomeCanvas has a bug whereby if its idle handler is not scheduled between
	   two calls to update_now, an assert will trip.  This wrapper works around
	   that problem by only calling update_now if the assert will not trip.

	   I think the GC bug is due to the fact that its code will reset need_update
	   and need_redraw to FALSE without checking to see if an idle handler is scheduled.
	   If one is scheduled, GC should probably remove it.
	*/

	GnomeCanvas* c = track_canvas->gobj ();
	if (c->need_update || c->need_redraw) {
		track_canvas->update_now ();
	}
}

double
Editor::horizontal_position () const
{
	return frame_to_unit (leftmost_frame);
}

void
Editor::set_canvas_cursor (Gdk::Cursor* cursor, bool save)
{
	if (save) {
		current_canvas_cursor = cursor;
	}

	Glib::RefPtr<Gdk::Window> win = track_canvas->get_window();

	if (win) {
	        track_canvas->get_window()->set_cursor (*cursor);
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
