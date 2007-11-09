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

*/

#include <ardour/audioregion.h>
#include <ardour/playlist.h>
#include <ardour/location.h>

#include <pbd/memento_command.h>

#include "editor.h"
#include "region_view.h"
#include "selection.h"
#include "keyboard.h"

#include "i18n.h"

using namespace ARDOUR;

void
Editor::kbd_driver (sigc::slot<void,GdkEvent*> theslot, bool use_track_canvas, bool use_time_canvas, bool can_select)
{
	gint x, y;
	double worldx, worldy;
	GdkEvent ev;
	Gdk::ModifierType mask;
	Glib::RefPtr<Gdk::Window> evw = track_canvas.get_window()->get_pointer (x, y, mask);
	bool doit = false;

	if (use_track_canvas && track_canvas_event_box.get_window()->get_pointer(x, y, mask) != 0) {
		doit = true;
	} else if (use_time_canvas && time_canvas_event_box.get_window()->get_pointer(x, y, mask)!= 0) {
		doit = true;
	}

	/* any use of "keyboard mouse buttons" invalidates an existing grab
	*/
	
	if (drag_info.item) {
		drag_info.item->ungrab (GDK_CURRENT_TIME);
		drag_info.item = 0;
	}

	if (doit) {

		if (entered_regionview && can_select) {
			selection->set (entered_regionview);
		}

		track_canvas.window_to_world (x, y, worldx, worldy);
		worldx += horizontal_adjustment.get_value();
		worldy += vertical_adjustment.get_value();

		ev.type = GDK_BUTTON_PRESS;
		ev.button.x = worldx;
		ev.button.y = worldy;
		ev.button.state = 0;  /* XXX correct? */

		theslot (&ev);
	}
}

void
Editor::set_edit_point (GdkEvent* event)
{
	if (selection->markers.empty()) {
		nframes64_t where = event_frame (event);

		snap_to (where);
		mouse_add_new_marker (where);

	} else {
		bool ignored;

		Location* loc = find_location_from_marker (selection->markers.front(), ignored);

		if (loc) {
			nframes64_t where = event_frame (event);
			snap_to (where);
			loc->move_to (where);
		}
	}
}

void
Editor::set_playhead_cursor (GdkEvent* event)
{
	if (entered_marker) {
		session->request_locate (entered_marker->position(), session->transport_rolling());
	} else {
		nframes_t pointer_frame = event_frame (event);
		
		if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
			snap_to (pointer_frame);
		}
		
		if (session) {
			session->request_locate (pointer_frame, session->transport_rolling());
		}
	}
}

void
Editor::kbd_set_edit_point ()
{
	kbd_driver (mem_fun(*this, &Editor::set_edit_point), true, true, false);
}	

void
Editor::kbd_set_playhead_cursor ()
{
	kbd_driver (mem_fun(*this, &Editor::set_playhead_cursor), true, true, false);
}

void
Editor::kbd_do_split (GdkEvent* ev)
{
	if (entered_regionview) {

		nframes_t where = event_frame (ev);
		snap_to (where);

		if (selection->regions.contains (entered_regionview)) {
			split_regions_at (where, selection->regions);
		} else {
			RegionSelection s;
			s.add (entered_regionview);
			split_regions_at (where, s);
		}

	} else if (entered_marker) {

		if (!selection->regions.empty()) {
			split_regions_at (entered_marker->position(), selection->regions);
		} else {
			RegionSelection rs;
			rs = get_regions_at (entered_marker->position(), selection->tracks);
			split_regions_at (entered_marker->position(), rs);
		}
	}
}

void
Editor::kbd_split ()
{
	kbd_driver (mem_fun(*this, &Editor::kbd_do_split), true, true, false);
}

void
Editor::kbd_mute_unmute_region ()
{
	if (entered_regionview) {
		begin_reversible_command (_("mute region"));
		XMLNode &before = entered_regionview->region()->playlist()->get_state();
		
		entered_regionview->region()->set_muted (!entered_regionview->region()->muted());
		
		XMLNode &after = entered_regionview->region()->playlist()->get_state();
		session->add_command (new MementoCommand<ARDOUR::Playlist>(*(entered_regionview->region()->playlist()), &before, &after));
		commit_reversible_command();
	}
}

void
Editor::kbd_set_sync_position ()
{
	kbd_driver (mem_fun(*this, &Editor::kbd_do_set_sync_position), true, true, false);
}

void
Editor::kbd_do_set_sync_position (GdkEvent* ev)
{
	if (entered_regionview) {
		nframes64_t where = event_frame (ev);
		snap_to (where);

		set_a_regions_sync_position (entered_regionview->region(), where);

	} else if (entered_marker) {

		if (!selection->regions.empty()) {
			set_a_regions_sync_position (selection->regions.front()->region(), entered_marker->position());
		}
	}
}

void
Editor::kbd_do_align (GdkEvent* ev, ARDOUR::RegionPoint what)
{
	if (entered_regionview) {

		nframes_t where = event_frame (ev);
		snap_to (where);

		if (selection->regions.contains (entered_regionview)) {
			align_selection (what, where, selection->regions);
		} else {
			RegionSelection s;
			s.add (entered_regionview);
			align_selection (what, where, s);
		}

	} else if (entered_marker) {

		if (!selection->regions.empty()) {
			align_selection (what, entered_marker->position(), selection->regions);
		} else {
			RegionSelection rs;
			rs = get_regions_at (entered_marker->position(), selection->tracks);
			align_selection (what, entered_marker->position(), rs);
		}
	}
}

void
Editor::kbd_do_align_relative (GdkEvent* ev, ARDOUR::RegionPoint what)
{
	if (entered_regionview) {

		nframes_t where = event_frame (ev);
		snap_to (where);

		if (selection->regions.contains (entered_regionview)) {
			align_selection_relative (what, where, selection->regions);
		} else {
			RegionSelection s;
			s.add (entered_regionview);
			align_selection_relative (what, where, s);
		}

	} else if (entered_marker) {

		if (!selection->regions.empty()) {
			align_selection_relative (what, entered_marker->position(), selection->regions);
		} else {
			RegionSelection rs;
			rs = get_regions_at (entered_marker->position(), selection->tracks);
			align_selection_relative (what, entered_marker->position(), rs);
		}
	}
}

void
Editor::kbd_align (ARDOUR::RegionPoint what)
{
	kbd_driver (bind (mem_fun(*this, &Editor::kbd_do_align), what));
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
