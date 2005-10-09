/*
    Copyright (C) 2004 Paul Davis 

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

#include <ardour/audioregion.h>

#include "editor.h"
#include "regionview.h"
#include "selection.h"

void
Editor::kbd_driver (sigc::slot<void,GdkEvent*> theslot, bool use_track_canvas, bool use_time_canvas, bool can_select)
{
	gint x, y;
	double dx, dy;
	GdkEvent ev;
	GdkModifierType mask;
	GdkWindow evw (track_canvas->get_window().get_pointer (x, y, mask));
	bool doit = false;

	if (use_track_canvas && gdk_window_get_pointer (track_canvas_event_box.get_window()->gobj(),
							&x, &y, &mask)) {
		doit = true;

	} else if (use_time_canvas && gdk_window_get_pointer (time_canvas_event_box.get_window()->gobj(),
							      &x, &y, &mask)) {
		doit = true;
	}

	if (doit) {

		if (entered_regionview && can_select) {
			selection->set (entered_regionview);
		}

		gnome_canvas_window_to_world (GNOME_CANVAS(track_gnome_canvas), x, y, &dx, &dy);

		ev.type = GDK_BUTTON_PRESS;
		ev.button.x = dx;
		ev.button.y = dy;
		ev.button.state = 0;  /* XXX correct? */

		theslot (&ev);
	}
}

void
Editor::kbd_set_playhead_cursor ()
{
	kbd_driver (mem_fun(*this, &Editor::set_playhead_cursor), true, true, false);
}

void
Editor::kbd_set_edit_cursor ()
{
	kbd_driver (mem_fun(*this, &Editor::set_edit_cursor), true, true, false);
}


void
Editor::kbd_do_split (GdkEvent* ev)
{
	jack_nframes_t where = event_frame (ev);

	if (entered_regionview) {
		if (selection->audio_regions.find (entered_regionview) != selection->audio_regions.end()) {
			split_regions_at (where, selection->audio_regions);
		} else {
			AudioRegionSelection s;
			s.add (entered_regionview);
			split_regions_at (where, s);
		}
	}
}

void
Editor::kbd_split ()
{
	kbd_driver (mem_fun(*this, &Editor::kbd_do_split), true, true, false);
}

void
Editor::kbd_do_align (GdkEvent* ev, ARDOUR::RegionPoint what)
{
	align (what);
}

void
Editor::kbd_align (ARDOUR::RegionPoint what)
{
	kbd_driver (bind (mem_fun(*this, &Editor::kbd_do_align), what));
}

void
Editor::kbd_do_align_relative (GdkEvent* ev, ARDOUR::RegionPoint what)
{
	align (what);
}

void
Editor::kbd_align_relative (ARDOUR::RegionPoint what)
{
	kbd_driver (bind (mem_fun(*this, &Editor::kbd_do_align), what), true, true, false);
}

void
Editor::kbd_do_brush (GdkEvent *ev)
{
	brush (event_frame (ev, 0, 0));
}

void
Editor::kbd_brush ()
{
	kbd_driver (mem_fun(*this, &Editor::kbd_do_brush), true, true, false);
}

void
Editor::kbd_do_audition (GdkEvent *ignored)
{
	audition_selected_region ();
}

void
Editor::kbd_audition ()
{
	kbd_driver (mem_fun(*this, &Editor::kbd_do_audition), true, false, true);
}
