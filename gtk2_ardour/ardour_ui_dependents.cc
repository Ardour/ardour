/*
    Copyright (C) 2000 Paul Davis 

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

/* this file exists solely to break compilation dependencies that
   would connect changes to the mixer or editor objects.
*/

#include <cstdio>
#include <pbd/error.h>
#include "ardour_ui.h"
#include "public_editor.h"
#include "mixer_ui.h"
#include "meter_bridge.h"
#include "keyboard.h"
#include "route_params_ui.h"
#include "i18n.h"

using namespace sigc;


namespace ARDOUR {
	class Session;
	class Route;
}

void
ARDOUR_UI::shutdown ()
{
	if (session) {
		delete session;
		session = 0;
	}

}

void
ARDOUR_UI::we_have_dependents ()
{
	setup_keybindings ();
}

void
ARDOUR_UI::setup_keybindings ()
{
	/* install default bindings */

	KeyboardTarget *defaults = new KeyboardTarget (editor->window(), X_("default"));

	XMLNode* keynode = ARDOUR::Config->get_keys();

	if (keynode != 0) {
		defaults->set_binding_state (*keynode);
		editor->set_binding_state (*keynode);
		mixer->set_binding_state (*keynode);
		meter_bridge->set_binding_state (*keynode);
	} else {
		error << _("keyboard_target: error setting binding state: invalid node") << endmsg;
	}
	
	/* use the default keyboard target for now */

	keyboard->set_default_target (defaults);
}

void
ARDOUR_UI::connect_dependents_to_session (ARDOUR::Session *s)
{
	editor->connect_to_session (s);
	mixer->connect_to_session (s);

	meter_bridge->set_session (s);
}

void
ARDOUR_UI::goto_editor_window ()
{
	editor->show_window ();
	editor->window().get_window().raise ();
}
void
ARDOUR_UI::goto_mixer_window ()
{
	mixer->show_window ();
	mixer->get_window().raise ();
}

gint
ARDOUR_UI::exit_on_main_window_close (GdkEventAny *ev)
{
	finish();
	return TRUE;
}
