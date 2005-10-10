/*
    Copyright (C) 2002 Paul Davis 

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

#include "editor.h"

#include <ardour/session.h>

using namespace std;
using namespace ARDOUR;

void
Editor::hscroll_slider_allocate (GtkAllocation *alloc)
{
	//edit_hscroll_slider_width = alloc->width;
	//edit_hscroll_slider_height = alloc->height ;

	if (session) {
		track_canvas_scroller.get_hadjustment()->set_upper (session->current_end_frame() / frames_per_unit);
	}

}

gint
Editor::hscroll_slider_expose (GdkEventExpose *ev)
{
	GdkRectangle draw_rect;
	GdkRectangle bar_rect;
	gint bar_max = edit_hscroll_slider_width - 2;

	bar_rect.y = 1;
	bar_rect.height = edit_hscroll_slider_height - 2;

	if (session) {
		bar_rect.width = (gint) floor (bar_max * ((canvas_width * frames_per_unit) / session->current_end_frame()));

		if (bar_rect.width > bar_max) {
			bar_rect.x = 1;
			bar_rect.width = bar_max;
		} else {
			bar_rect.x = 1 + (gint) floor (bar_max * ((double) leftmost_frame / session->current_end_frame()));
		}
		
	} else {
		bar_rect.x = 1;
		bar_rect.width = bar_max;
	}

	/* make sure we can see the bar at all times, and have enough to do zoom-trim on */

	bar_rect.width = max ((guint16) (edit_hscroll_edge_width+5), (guint16) bar_rect.width);

	gdk_rectangle_intersect (&ev->area, &bar_rect, &draw_rect);

	gtk_paint_box (edit_hscroll_slider.get_style()->gobj(),
		       edit_hscroll_slider.get_window()->gobj(),
		       GTK_STATE_ACTIVE, 
		       GTK_SHADOW_IN, 
		       &ev->area, 
		       GTK_WIDGET(edit_hscroll_slider.gobj()),
		       "trough",
		       0, 0, -1, -1);

	gtk_paint_box (edit_hscroll_slider.get_style()->gobj(),
		       edit_hscroll_slider.get_window()->gobj(),
		       GTK_STATE_NORMAL, 
		       GTK_SHADOW_OUT, 
		       &draw_rect,
		       GTK_WIDGET(edit_hscroll_slider.gobj()),
		       "hscale",
		       bar_rect.x, bar_rect.y, bar_rect.width, bar_rect.height);


	return TRUE;
}

gint
Editor::hscroll_slider_button_press (GdkEventButton *ev)
{
	if (!session) {
		return TRUE;
	}

	edit_hscroll_dragging = true;
	//cerr << "PRESS" << endl;

	return TRUE;
	
	gint start;
	gint width;
	gint end;
	gint x;

	start = (gint) floor (edit_hscroll_slider_width * ((double) leftmost_frame / session->current_end_frame()));
	width = (gint) floor (edit_hscroll_slider_width * ((canvas_width * frames_per_unit) / session->current_end_frame()));

	end = start + width - 1;
	x = (gint) max (0.0, ev->x);
	
	if (x >= start && x <= end) {
		
		edit_hscroll_drag_last = x;
		edit_hscroll_dragging = true;
		Gtk::Main::grab_add (edit_hscroll_slider);
	}

	return TRUE;
}

gint
Editor::hscroll_slider_button_release (GdkEventButton *ev)
{
	if (!session) {
		return TRUE;
	}

	gint start;
	gint width;
	gint end;
	gint x;
	gint bar_max = edit_hscroll_slider_width - 2;
	jack_nframes_t new_leftmost = 0;

	//cerr << "RELESAE" << endl;

	if (edit_hscroll_dragging) {
		// lets do a tempo redisplay now only, because it is dog slow
		tempo_map_changed (Change (0));
		edit_hscroll_dragging = false;
	}
	
	return TRUE;

	
	start = (gint) floor (bar_max * ((double) leftmost_frame / session->current_end_frame()));
	width = (gint) floor (bar_max * ((canvas_width * frames_per_unit) / session->current_end_frame()));

	end = start + width - 1;
	x = (gint) max (0.0, ev->x);

	if (!edit_hscroll_dragging) {

		new_leftmost = (jack_nframes_t) floor (((double) x/bar_max) * session->current_end_frame());
		reposition_x_origin (new_leftmost);
	}

	if (edit_hscroll_dragging) {
		// lets do a tempo redisplay now only, because it is dog slow
		tempo_map_changed (Change (0));
		edit_hscroll_dragging = false;
		Gtk::Main::grab_remove (edit_hscroll_slider);
	} 

	return TRUE;
}

gint
Editor::hscroll_slider_motion (GdkEventMotion *ev)
{
	gint x, y;
	Gdk::ModifierType state;
	gint bar_max = edit_hscroll_slider_width - 2;

	if (!session || !edit_hscroll_dragging) {
		return TRUE;
	}

	edit_hscroll_slider.get_window()->get_pointer (x, y, state);

	jack_nframes_t new_frame;
	jack_nframes_t frames;
	double distance;
	double fract;

	distance = x - edit_hscroll_drag_last;
	fract = fabs (distance) / bar_max;
	frames = (jack_nframes_t) floor (session->current_end_frame() * fract);

	if (distance < 0) {
		if (leftmost_frame < frames) {
			new_frame = 0;
		} else {
			new_frame = leftmost_frame - frames;
		}
	} else {
		if (leftmost_frame > max_frames - frames) {
			new_frame = max_frames;
		} else {
			new_frame = leftmost_frame + frames;
		}
	}

	if (new_frame != leftmost_frame) {
		reposition_x_origin (new_frame);
	}

	edit_hscroll_drag_last = x;
	
	return TRUE;
}

void
Editor::update_hscroller ()
{
	//edit_hscroll_slider.queue_draw ();
//	if (session) {
// 		track_canvas_scroller.get_hadjustment()->set_upper (session->current_end_frame() / frames_per_unit);
// 		track_canvas_scroller.get_hadjustment()->set_value (leftmost_frame/frames_per_unit);
//	}
}

gint
Editor::hscroll_left_arrow_button_press (GdkEventButton *ev)
{
	if (!session) {
		return FALSE;
	}

	start_canvas_autoscroll (-1);
	return TRUE;
}

gint
Editor::hscroll_right_arrow_button_press (GdkEventButton *ev)
{
	if (!session) {
		return FALSE;
	}

	start_canvas_autoscroll (1);
	return TRUE;
}

gint
Editor::hscroll_left_arrow_button_release (GdkEventButton *ev)
{
	if (!session) {
		return FALSE;
	}

	stop_canvas_autoscroll ();
	return TRUE;
}

gint
Editor::hscroll_right_arrow_button_release (GdkEventButton *ev)
{
	if (!session) {
		return FALSE;
	}

	stop_canvas_autoscroll ();
	return TRUE;
}
