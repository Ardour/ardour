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

#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/audioregion.h"
#include "ardour/playlist.h"
#include "ardour/session.h"
#include "ardour/location.h"

#include "editor.h"
#include "region_view.h"
#include "selection.h"
#include "keyboard.h"
#include "editor_drag.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

void
Editor::kbd_driver (sigc::slot<void,GdkEvent*> theslot, bool use_track_canvas, bool use_time_canvas, bool can_select)
{
	gint x, y;
	double worldx, worldy;
	GdkEvent ev;
	Gdk::ModifierType mask;
	Glib::RefPtr<Gdk::Window> evw = track_canvas->get_window()->get_pointer (x, y, mask);
	bool doit = false;

	if (use_track_canvas && track_canvas_event_box.get_window()->get_pointer(x, y, mask) != 0) {
		doit = true;
	} else if (use_time_canvas && time_canvas_event_box.get_window()->get_pointer(x, y, mask)!= 0) {
		doit = true;
	}

	/* any use of "keyboard mouse buttons" invalidates an existing grab
	*/

	if (_drags->active ()) {
		_drags->abort ();
	}

	if (doit) {

		if (entered_regionview && can_select) {
			selection->set (entered_regionview);
		}

		track_canvas->window_to_world (x, y, worldx, worldy);
		worldx += _horizontal_position;
		worldy += vertical_adjustment.get_value();

		ev.type = GDK_BUTTON_PRESS;
		ev.button.x = worldx;
		ev.button.y = worldy;
		ev.button.state = 0;  /* XXX correct? */

		theslot (&ev);
	}
}

void
Editor::kbd_mute_unmute_region ()
{
	if (!selection->regions.empty ()) {

		if (selection->regions.size() > 1) {
			begin_reversible_command (_("mute regions"));
		} else {
			begin_reversible_command (_("mute region"));
		}

		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

			(*i)->region()->playlist()->clear_history ();
			(*i)->region()->set_muted (!(*i)->region()->muted ());
			_session->add_command (new StatefulDiffCommand ((*i)->region()->playlist()));

		}

		commit_reversible_command ();

	} else if (entered_regionview) {

		begin_reversible_command (_("mute region"));
                entered_regionview->region()->playlist()->clear_history ();
		entered_regionview->region()->set_muted (!entered_regionview->region()->muted());
		_session->add_command (new StatefulDiffCommand (entered_regionview->region()->playlist()));
		commit_reversible_command();

	}
}

void
Editor::kbd_do_brush (GdkEvent *ev)
{
	brush (event_frame (ev, 0, 0));
}

void
Editor::kbd_brush ()
{
	kbd_driver (sigc::mem_fun(*this, &Editor::kbd_do_brush), true, true, false);
}

