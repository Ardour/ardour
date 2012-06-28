/*
    Copyright (C) 2001 Paul Davis

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

#include "pbd/error.h"
#include "pbd/file_utils.h"

#include "ardour/filesystem_paths.h"

#include "ardour_ui.h"
#include "keyboard.h"
#include "opts.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;
using Gtkmm2ext::Keyboard;

static void
accel_map_changed (GtkAccelMap* /*map*/,
		   gchar* /*path*/,
		   guint /*key*/,
		   GdkModifierType /*mod*/,
		   gpointer keyboard)
{
	ArdourKeyboard* me = (ArdourKeyboard*)keyboard;
	Keyboard::keybindings_changed ();
	me->ui.setup_tooltips ();
}

void
ArdourKeyboard::setup_keybindings ()
{
	using namespace ARDOUR_COMMAND_LINE;
	string default_bindings = "mnemonic-us.bindings";
	vector<string> strs;

	binding_files.clear ();

	ARDOUR::find_bindings_files (binding_files);

	/* set up the per-user bindings path */

	user_keybindings_path = Glib::build_filename (user_config_directory(), "ardour.bindings");

	if (Glib::file_test (user_keybindings_path, Glib::FILE_TEST_EXISTS)) {
		std::pair<string,string> newpair;
		newpair.first = _("your own");
		newpair.second = user_keybindings_path;
		binding_files.insert (newpair);
	}

	/* check to see if they gave a style name ("SAE", "ergonomic") or
	   an actual filename (*.bindings)
	*/

	if (!keybindings_path.empty() && keybindings_path.find (".bindings") == string::npos) {

		// just a style name - allow user to
		// specify the layout type.

		char* layout;

		if ((layout = getenv ("ARDOUR_KEYBOARD_LAYOUT")) != 0 && layout[0] != '\0') {

			/* user-specified keyboard layout */

			keybindings_path += '-';
			keybindings_path += layout;

		} else {

			/* default to US/ANSI - we have to pick something */

			keybindings_path += "-us";
		}

		keybindings_path += ".bindings";
	}

	if (keybindings_path.empty()) {

		/* no path or binding name given: check the user one first */

		if (!Glib::file_test (user_keybindings_path, Glib::FILE_TEST_EXISTS)) {

			keybindings_path = "";

		} else {

			keybindings_path = user_keybindings_path;
		}
	}

	/* if we still don't have a path at this point, use the default */

	if (keybindings_path.empty()) {
		keybindings_path = default_bindings;
	}

	while (true) {

		if (!Glib::path_is_absolute (keybindings_path)) {

			/* not absolute - look in the usual places */
			std::string keybindings_file;

			if ( ! find_file_in_search_path (ardour_config_search_path(), keybindings_path, keybindings_file)) {

				if (keybindings_path == default_bindings) {
					error << string_compose (_("Default keybindings not found - %1 will be hard to use!"), PROGRAM_NAME) << endmsg;
					return;
				} else {
					warning << string_compose (_("Key bindings file \"%1\" not found. Default bindings used instead"),
								   keybindings_path)
						<< endmsg;
					keybindings_path = default_bindings;
				}

			} else {

				/* use it */

				keybindings_path = keybindings_file;
				break;

			}

		} else {

			/* path is absolute already */

			if (!Glib::file_test (keybindings_path, Glib::FILE_TEST_EXISTS)) {
				if (keybindings_path == default_bindings) {
					error << string_compose (_("Default keybindings not found - %1 will be hard to use!"), PROGRAM_NAME) << endmsg;
					return;
				} else {
					warning << string_compose (_("Key bindings file \"%1\" not found. Default bindings used instead"),
								   keybindings_path)
						<< endmsg;
					keybindings_path = default_bindings;
				}

			} else {
				break;
			}
		}
	}

	load_keybindings (keybindings_path);

	/* catch changes */

	GtkAccelMap* accelmap = gtk_accel_map_get();
	g_signal_connect (accelmap, "changed", (GCallback) accel_map_changed, this);
}

Selection::Operation
ArdourKeyboard::selection_type (guint state)
{
	/* note that there is no modifier for "Add" */

	if (modifier_state_equals (state, RangeSelectModifier)) {
		return Selection::Extend;
	} else if (modifier_state_equals (state, PrimaryModifier)) {
		return Selection::Toggle;
	} else {
		return Selection::Set;
	}
}


