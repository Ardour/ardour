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

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/basename.h"

#include "ardour/filesystem_paths.h"

#include "ardour_ui.h"
#include "public_editor.h"
#include "keyboard.h"
#include "opts.h"
#include "ui_config.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;
using Gtkmm2ext::Keyboard;

#ifdef GTKOSX
guint ArdourKeyboard::constraint_mod = Keyboard::PrimaryModifier;
#else
guint ArdourKeyboard::constraint_mod = Keyboard::SecondaryModifier;
#endif
guint ArdourKeyboard::trim_contents_mod = Keyboard::PrimaryModifier;
guint ArdourKeyboard::trim_overlap_mod = Keyboard::TertiaryModifier;
guint ArdourKeyboard::trim_anchored_mod = Keyboard::TertiaryModifier;
guint ArdourKeyboard::fine_adjust_mod = Keyboard::SecondaryModifier;
guint ArdourKeyboard::push_points_mod = Keyboard::PrimaryModifier;
guint ArdourKeyboard::note_size_relative_mod = Keyboard::PrimaryModifier;

void
ArdourKeyboard::find_bindings_files (map<string,string>& files)
{
	vector<std::string> found;
	Searchpath spath = ardour_config_search_path();

	find_files_matching_pattern (found, spath, string_compose ("*.%1", Keyboard::binding_filename_suffix));

	if (found.empty()) {
		return;
	}

	for (vector<std::string>::iterator x = found.begin(); x != found.end(); ++x) {
		std::string path(*x);
		pair<string,string> namepath;
		namepath.second = path;
		namepath.first = PBD::basename_nosuffix (path);
		files.insert (namepath);
	}
}

void
ArdourKeyboard::setup_keybindings ()
{
	using namespace ARDOUR_COMMAND_LINE;
	string default_bindings = string_compose ("%1%2", UIConfiguration::instance().get_default_bindings(), Keyboard::binding_filename_suffix);
	vector<string> strs;

	binding_files.clear ();

	find_bindings_files (binding_files);

	/* set up the per-user bindings path */

	string lowercase_program_name = downcase (string(PROGRAM_NAME));

	user_keybindings_path = Glib::build_filename (user_config_directory(), lowercase_program_name + binding_filename_suffix);

	if (Glib::file_test (user_keybindings_path, Glib::FILE_TEST_EXISTS)) {
		std::pair<string,string> newpair;
		newpair.first = _("your own");
		newpair.second = user_keybindings_path;
		binding_files.insert (newpair);
	}

	/* check to see if they gave a style name ("ergonomic") or an actual filename (*.bindings)
	*/

	if (!keybindings_path.empty() && keybindings_path.find (binding_filename_suffix) == string::npos) {

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

		keybindings_path += binding_filename_suffix;
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

	cerr << "KP is " << keybindings_path << endl;

	while (true) {

		if (!Glib::path_is_absolute (keybindings_path)) {

			/* not absolute - look in the usual places */
			std::string keybindings_file;

			if (!find_file (ardour_config_search_path(), keybindings_path, keybindings_file)) {

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
					keybindings_path = default_bindings;
				}

			} else {
				break;
			}
		}
	}

	info << string_compose (_("Loading keybindings from %1"), keybindings_path) << endmsg;

	load_keybindings (keybindings_path);

	/* catch changes made via some GTK mechanism */

	// GtkAccelMap* accelmap = gtk_accel_map_get();
	// g_signal_connect (accelmap, "changed", (GCallback) accel_map_changed, this);
}

XMLNode&
ArdourKeyboard::get_state (void)
{
	XMLNode* node = &Keyboard::get_state ();
	char buf[32];

	snprintf (buf, sizeof (buf), "%d", constraint_mod);
	node->add_property ("constraint-modifier", buf);
	snprintf (buf, sizeof (buf), "%d", trim_contents_mod);
	node->add_property ("trim-contents-modifier", buf);
	snprintf (buf, sizeof (buf), "%d", trim_overlap_mod);
	node->add_property ("trim-overlap-modifier", buf);
	snprintf (buf, sizeof (buf), "%d", trim_anchored_mod);
	node->add_property ("trim-anchored-modifier", buf);
	snprintf (buf, sizeof (buf), "%d", fine_adjust_mod);
	node->add_property ("fine-adjust-modifier", buf);
	snprintf (buf, sizeof (buf), "%d", push_points_mod);
	node->add_property ("push-points-modifier", buf);
	snprintf (buf, sizeof (buf), "%d", note_size_relative_mod);
	node->add_property ("note-size-relative-modifier", buf);

	return *node;
}

int
ArdourKeyboard::set_state (const XMLNode& node, int version)
{
	const XMLProperty* prop;

	if ((prop = node.property ("constraint-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &constraint_mod);
	}

	if ((prop = node.property ("trim-contents-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &trim_contents_mod);
	}

	if ((prop = node.property ("trim-overlap-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &trim_overlap_mod);
	}

	if ((prop = node.property ("trim-anchored-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &trim_anchored_mod);
	}

	if ((prop = node.property ("fine-adjust-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &fine_adjust_mod);
	}

	if ((prop = node.property ("push-points-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &push_points_mod);
	}

	if ((prop = node.property ("note-size-relative-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &note_size_relative_mod);
	}

	return Keyboard::set_state (node, version);
}

/* Snap and snap delta modifiers may contain each other, so we use the
 * following two methods to sort that out:
 */
bool
ArdourKeyboard::indicates_snap (guint state)
{
	const bool contains_s = Keyboard::modifier_state_contains (state, Keyboard::snap_modifier ());
	const bool contains_d = Keyboard::modifier_state_contains (state, Keyboard::snap_delta_modifier ());
	const bool s_contains_d = Keyboard::modifier_state_contains (Keyboard::snap_modifier (), Keyboard::snap_delta_modifier ());

	return  (contains_s && ((contains_d && s_contains_d) || !contains_d));
}

bool
ArdourKeyboard::indicates_snap_delta (guint state)
{
	const bool contains_d = Keyboard::modifier_state_contains (state, Keyboard::snap_delta_modifier ());
	const bool contains_s = Keyboard::modifier_state_contains (state, Keyboard::snap_modifier ());
	const bool d_contains_s = Keyboard::modifier_state_contains (Keyboard::snap_delta_modifier (), Keyboard::snap_modifier ());

	return (contains_d && ((contains_s && d_contains_s) || !contains_s));
}

void
ArdourKeyboard::set_constraint_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~constraint_mod);
	constraint_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | constraint_mod);
}

void
ArdourKeyboard::set_trim_contents_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~trim_contents_mod);
	trim_contents_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | trim_contents_mod);
}

void
ArdourKeyboard::set_trim_overlap_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~trim_overlap_mod);
	trim_overlap_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | trim_overlap_mod);
}

void
ArdourKeyboard::set_trim_anchored_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~trim_anchored_mod);
	trim_anchored_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | trim_anchored_mod);
}

void
ArdourKeyboard::set_fine_adjust_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~fine_adjust_mod);
	fine_adjust_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | fine_adjust_mod);
}

void
ArdourKeyboard::set_push_points_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~push_points_mod);
	push_points_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | push_points_mod);
}

void
ArdourKeyboard::set_note_size_relative_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~note_size_relative_mod);
	note_size_relative_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | note_size_relative_mod);
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
