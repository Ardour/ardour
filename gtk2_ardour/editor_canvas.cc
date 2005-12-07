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

    $Id$
*/

#include <libgnomecanvasmm/init.h>
#include <jack/types.h>

#include "ardour_ui.h"
#include "editor.h"
#include "waveview.h"
#include "simplerect.h"
#include "simpleline.h"
#include "imageframe.h"
#include "waveview_p.h"
#include "simplerect_p.h"
#include "simpleline_p.h"
#include "imageframe_p.h"
#include "canvas_impl.h"
#include "editing.h"
#include "rgb_macros.h"
#include "utils.h"
#include "time_axis_view.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace Editing;

/* XXX this is a hack. it ought to be the maximum value of an jack_nframes_t */

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
	Glib::wrap_register(gnome_canvas_imageframe_get_type(), &Gnome::Canvas::ImageFrame_Class::wrap_new);
	
	// Register the gtkmm gtypes:

	(void) Gnome::Canvas::WaveView::get_type();
	(void) Gnome::Canvas::SimpleLine::get_type();
	(void) Gnome::Canvas::SimpleRect::get_type();
	(void) Gnome::Canvas::ImageFrame::get_type();
} 

void
Editor::initialize_canvas ()
{
	ArdourCanvas::init ();
	ardour_canvas_type_init ();

	/* don't try to center the canvas */

	track_canvas.set_center_scroll_region (false);

	track_canvas.signal_event().connect (bind (mem_fun (*this, &Editor::track_canvas_event), (ArdourCanvas::Item*) 0));
	track_canvas.set_name ("EditorMainCanvas");
	track_canvas.add_events (Gdk::POINTER_MOTION_HINT_MASK);
	track_canvas.signal_leave_notify_event().connect (mem_fun(*this, &Editor::left_track_canvas));
	
	/* set up drag-n-drop */
	vector<Gtk::TargetEntry> target_table;
	
	target_table.push_back (TargetEntry ("STRING"));
	target_table.push_back (TargetEntry ("text/plain"));
	target_table.push_back (TargetEntry ("text/uri-list"));
	target_table.push_back (TargetEntry ("application/x-rootwin-drop"));

	// GTK2FIX
	// track_canvas.drag_dest_set (target_table, DEST_DEFAULT_ALL, GdkDragAction (Gdk::ACTION_COPY|Gdk::ACTION_MOVE));
	// track_canvas.signal_drag_data_received().connect (mem_fun(*this, &Editor::track_canvas_drag_data_received));

	/* stuff for the verbose canvas cursor */

	Pango::FontDescription font = get_font_for_style (N_("VerboseCanvasCursor"));

	verbose_canvas_cursor = new ArdourCanvas::Text (*track_canvas.root());
	verbose_canvas_cursor->property_font_desc() = font;
	// GTK2FIX
	// verbose_canvas_cursor->property_anchor() = GTK_ANCHOR_NW;
	verbose_canvas_cursor->property_fill_color_rgba() = color_map[cVerboseCanvasCursor];
	
	verbose_cursor_visible = false;
	
	/* a group to hold time (measure) lines */
	
	time_line_group = new ArdourCanvas::Group (*track_canvas.root(), 0.0, 0.0);
	cursor_group = new ArdourCanvas::Group (*track_canvas.root(), 0.0, 0.0);
	
	time_canvas.set_name ("EditorTimeCanvas");
	time_canvas.add_events (Gdk::POINTER_MOTION_HINT_MASK);
	
	meter_group = new ArdourCanvas::Group (*time_canvas.root(), 0.0, 0.0);
	tempo_group = new ArdourCanvas::Group (*time_canvas.root(), 0.0, 0.0);
	marker_group = new ArdourCanvas::Group (*time_canvas.root(), 0.0, timebar_height * 2.0);
	range_marker_group = new ArdourCanvas::Group (*time_canvas.root(), 0.0, timebar_height * 3.0);
	transport_marker_group = new ArdourCanvas::Group (*time_canvas.root(), 0.0, timebar_height * 4.0);
	
	tempo_bar = new ArdourCanvas::SimpleRect (*tempo_group, 0.0, 0.0, max_canvas_coordinate, timebar_height);
	tempo_bar->property_fill_color_rgba() = color_map[cTempoBar];
	tempo_bar->property_outline_pixels() = 0;
	
	meter_bar = new ArdourCanvas::SimpleRect (*meter_group, 0.0, 0.0, max_canvas_coordinate, timebar_height);
	meter_bar->property_fill_color_rgba() = color_map[cMeterBar];
	meter_bar->property_outline_pixels() = 0;
	
	marker_bar = new ArdourCanvas::SimpleRect (*marker_group, 0.0, 0.0, max_canvas_coordinate, timebar_height);
	marker_bar->property_fill_color_rgba() = color_map[cMarkerBar];
	marker_bar->property_outline_pixels() = 0;
	
	range_marker_bar = new ArdourCanvas::SimpleRect (*range_marker_group, 0.0, 0.0, max_canvas_coordinate, timebar_height);
	range_marker_bar->property_fill_color_rgba() = color_map[cRangeMarkerBar];
	range_marker_bar->property_outline_pixels() = 0;
	
	transport_marker_bar = new ArdourCanvas::SimpleRect (*transport_marker_group, 0.0, 0.0, max_canvas_coordinate, timebar_height);
	transport_marker_bar->property_fill_color_rgba() = color_map[cTransportMarkerBar];
	transport_marker_bar->property_outline_pixels() = 0;
	
	range_bar_drag_rect = new ArdourCanvas::SimpleRect (*range_marker_group, 0.0, 0.0, max_canvas_coordinate, timebar_height);
	range_bar_drag_rect->property_fill_color_rgba() = color_map[cRangeDragBarRectFill];
	range_bar_drag_rect->property_outline_color_rgba() = color_map[cRangeDragBarRect];
	range_bar_drag_rect->property_outline_pixels() = 0;
	range_bar_drag_rect->hide ();
	
	transport_bar_drag_rect = new ArdourCanvas::SimpleRect (*transport_marker_group, 0.0, 0.0, max_canvas_coordinate, timebar_height);
	transport_bar_drag_rect ->property_fill_color_rgba() = color_map[cTransportDragRectFill];
	transport_bar_drag_rect->property_outline_color_rgba() = color_map[cTransportDragRect];
	transport_bar_drag_rect->property_outline_pixels() = 0;
	transport_bar_drag_rect->hide ();
	
	marker_drag_line_points.push_back(Gnome::Art::Point(0.0, 0.0));
	marker_drag_line_points.push_back(Gnome::Art::Point(0.0, 0.0));

	marker_drag_line = new ArdourCanvas::Line (*track_canvas.root());
	marker_drag_line->property_width_pixels() = 1;
	marker_drag_line->property_fill_color_rgba() = color_map[cMarkerDragLine];
	marker_drag_line->property_points() = marker_drag_line_points;
	marker_drag_line->hide();

	range_marker_drag_rect = new ArdourCanvas::SimpleRect (*track_canvas.root(), 0.0, 0.0, 0.0, 0.0);
	range_marker_drag_rect->property_fill_color_rgba() = color_map[cRangeDragRectFill];
	range_marker_drag_rect->property_outline_color_rgba() = color_map[cRangeDragRect];
	range_marker_drag_rect->hide ();
	
	transport_loop_range_rect = new ArdourCanvas::SimpleRect (*time_line_group, 0.0, 0.0, 0.0, 0.0);
	transport_loop_range_rect->property_fill_color_rgba() = color_map[cTransportLoopRectFill];
	transport_loop_range_rect->property_outline_color_rgba() = color_map[cTransportLoopRect];
	transport_loop_range_rect->property_outline_pixels() = 1;
	transport_loop_range_rect->hide();

	transport_punch_range_rect = new ArdourCanvas::SimpleRect (*time_line_group, 0.0, 0.0, 0.0, 0.0);
	transport_punch_range_rect->property_fill_color_rgba() = color_map[cTransportPunchRectFill];
	transport_punch_range_rect->property_outline_color_rgba() = color_map[cTransportPunchRect];
	transport_punch_range_rect->property_outline_pixels() = 0;
	transport_punch_range_rect->hide();
	
	transport_loop_range_rect->lower_to_bottom (); // loop on the bottom

	transport_punchin_line = new ArdourCanvas::SimpleLine (*time_line_group);
	transport_punchin_line->property_x1() = 0.0;
	transport_punchin_line->property_y1() = 0.0;
	transport_punchin_line->property_x2() = 0.0;
	transport_punchin_line->property_y2() = 0.0;
	transport_punchin_line->property_color_rgba() = color_map[cPunchInLine];
	transport_punchin_line->hide ();
	
	transport_punchout_line  = new ArdourCanvas::SimpleLine (*time_line_group);
	transport_punchout_line->property_x1() = 0.0;
	transport_punchout_line->property_y1() = 0.0;
	transport_punchout_line->property_x2() = 0.0;
	transport_punchout_line->property_y2() = 0.0;
	transport_punchout_line->property_color_rgba() = color_map[cPunchOutLine];
	transport_punchout_line->hide();
	
	// used to show zoom mode active zooming
	zoom_rect = new ArdourCanvas::SimpleRect (*track_canvas.root(), 0.0, 0.0, 0.0, 0.0);
	zoom_rect->property_fill_color_rgba() = color_map[cZoomRectFill];
	zoom_rect->property_outline_color_rgba() = color_map[cZoomRect];
	zoom_rect->property_outline_pixels() = 1;
	zoom_rect->hide();
	
	zoom_rect->signal_event().connect (bind (mem_fun (*this, &Editor::canvas_zoom_rect_event), (ArdourCanvas::Item*) 0));
	
	// used as rubberband rect
	rubberband_rect = new ArdourCanvas::SimpleRect (*track_canvas.root(), 0.0, 0.0, 0.0, 0.0);
	rubberband_rect->property_outline_color_rgba() = color_map[cRubberBandRect];
	rubberband_rect->property_fill_color_rgba() = (guint32) color_map[cRubberBandRectFill];
	rubberband_rect->property_outline_pixels() = 1;
	rubberband_rect->hide();
	
	tempo_bar->signal_event().connect (bind (mem_fun (*this, &Editor::canvas_tempo_bar_event), tempo_bar));
	meter_bar->signal_event().connect (bind (mem_fun (*this, &Editor::canvas_meter_bar_event), meter_bar));
	marker_bar->signal_event().connect (bind (mem_fun (*this, &Editor::canvas_marker_bar_event), marker_bar));
	range_marker_bar->signal_event().connect (bind (mem_fun (*this, &Editor::canvas_range_marker_bar_event), range_marker_bar));
	transport_marker_bar->signal_event().connect (bind (mem_fun (*this, &Editor::canvas_transport_marker_bar_event), transport_marker_bar));
	
	/* separator lines */
	
	tempo_line = new ArdourCanvas::SimpleLine (*tempo_group, 0, timebar_height, max_canvas_coordinate, timebar_height);
	tempo_line->property_color_rgba() = RGBA_TO_UINT (0,0,0,255);

	meter_line = new ArdourCanvas::SimpleLine (*meter_group, 0, timebar_height, max_canvas_coordinate, timebar_height);
	meter_line->property_color_rgba() = RGBA_TO_UINT (0,0,0,255);

	marker_line = new ArdourCanvas::SimpleLine (*marker_group, 0, timebar_height, max_canvas_coordinate, timebar_height);
	marker_line->property_color_rgba() = RGBA_TO_UINT (0,0,0,255);
	
	range_marker_line = new ArdourCanvas::SimpleLine (*range_marker_group, 0, timebar_height, max_canvas_coordinate, timebar_height);
	range_marker_line->property_color_rgba() = RGBA_TO_UINT (0,0,0,255);

	transport_marker_line = new ArdourCanvas::SimpleLine (*transport_marker_group, 0, timebar_height, max_canvas_coordinate, timebar_height);
	transport_marker_line->property_color_rgba() = RGBA_TO_UINT (0,0,0,255);

	ZoomChanged.connect (bind (mem_fun(*this, &Editor::update_loop_range_view), false));
	ZoomChanged.connect (bind (mem_fun(*this, &Editor::update_punch_range_view), false));
	
	double time_height = timebar_height * 5;
	double time_width = FLT_MAX/frames_per_unit;
	time_canvas.set_scroll_region(0.0, 0.0, time_width, time_height);
	
	edit_cursor = new Cursor (*this, "blue", &Editor::canvas_edit_cursor_event);
	playhead_cursor = new Cursor (*this, "red", &Editor::canvas_playhead_cursor_event);
	
	track_canvas.signal_size_allocate().connect (mem_fun(*this, &Editor::track_canvas_allocate));
}

void
Editor::track_canvas_allocate (Gtk::Allocation alloc)
{
	static bool first_time = true;

	canvas_width = alloc.get_width();
	canvas_height = alloc.get_height();

	if (session == 0 && !ARDOUR_UI::instance()->will_create_new_session_automatically()) {

		/* this mess of code is here to find out how wide this text is and
		   position the message in the center of the editor window.
		*/
			
		int pixel_height;
		int pixel_width;
		
		ustring msg = string_compose ("<span face=\"sans\" style=\"normal\" weight=\"bold\" size=\"x-large\">%1%2</span>",
					   _("Start a new session\n"), _("via Session menu"));

		RefPtr<Pango::Layout> layout = create_pango_layout (msg);
		Pango::FontDescription font = get_font_for_style (N_("FirstActionMessage"));
		layout->get_pixel_size (pixel_width, pixel_height);
			
		if (first_action_message == 0) {
			
			first_action_message = new ArdourCanvas::Text (*track_canvas.root());
			first_action_message->property_font_desc() = font;
			first_action_message->property_fill_color_rgba() = color_map[cFirstActionMessage];
			first_action_message->property_x() = (canvas_width - pixel_width) / 2.0;
			first_action_message->property_y() = (canvas_height/2.0) - pixel_height;
			first_action_message->property_anchor() = ANCHOR_NORTH_WEST;
			first_action_message->property_markup() = msg;
			
		} else {

			/* center it */
			first_action_message->property_x() = (canvas_width - pixel_width) / 2.0;
			first_action_message->property_y() = (canvas_height/2.0) - pixel_height;
		}
	}

	zoom_range_clock.set ((jack_nframes_t) floor ((canvas_width * frames_per_unit)));
	edit_cursor->set_position (edit_cursor->current_frame);
	playhead_cursor->set_position (playhead_cursor->current_frame);

	double last_canvas_unit = ceil ((double) max_frames / frames_per_unit);
	
	track_canvas.set_scroll_region ( 0.0, 0.0, max (last_canvas_unit, canvas_width), canvas_height);
	time_canvas.set_scroll_region ( 0.0, 0.0, max (last_canvas_unit, canvas_width), canvas_height);

	if (edit_cursor) edit_cursor->set_length (canvas_height);
	if (playhead_cursor) playhead_cursor->set_length (canvas_height);

	if (marker_drag_line) {
		marker_drag_line_points.back().set_x(canvas_height);
		marker_drag_line->property_points() = marker_drag_line_points;
	}

	if (range_marker_drag_rect) {
		range_marker_drag_rect->property_y1() = 0.0;
		range_marker_drag_rect->property_y2() = canvas_height;
	}

	if (transport_loop_range_rect) {
		transport_loop_range_rect->property_y1() = 0.0;
		transport_loop_range_rect->property_y2() = canvas_height;
	}

	if (transport_punch_range_rect) {
		transport_punch_range_rect->property_y1() = 0.0;
		transport_punch_range_rect->property_y2() = canvas_height;
	}

	if (transport_punchin_line) {
		transport_punchin_line->property_y1() = 0.0;
		transport_punchin_line->property_y2() = canvas_height;
	}

	if (transport_punchout_line) {
		transport_punchout_line->property_y1() = 0.0;
		transport_punchout_line->property_y2() = canvas_height;
	}
		
	update_fixed_rulers ();

	if (is_visible() && first_time) {
		tempo_map_changed (Change (0));
		first_time = false;
	} else {
		redisplay_tempo ();
	}
	
	Resized (); /* EMIT_SIGNAL */
}

void
Editor::reset_scrolling_region (Gtk::Allocation* alloc)
{
}

bool
Editor::track_canvas_map_handler (GdkEventAny* ev)
{
	track_canvas_scroller.get_window()->set_cursor (*current_canvas_cursor);
	return false;
}

bool
Editor::time_canvas_map_handler (GdkEventAny* ev)
{
	time_canvas_scroller.get_window()->set_cursor (*timebar_cursor);
	return false;
}

