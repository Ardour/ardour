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

#include <ardour/ardour.h>

#include "ardour_ui.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include <ctype.h>

#include <gtkmm/accelmap.h>

#include <gdk/gdkkeysyms.h>
#include <pbd/error.h>

#include "keyboard.h"
#include "gui_thread.h"
#include "opts.h"

#include "i18n.h"

using namespace PBD;
using namespace ARDOUR;

#define KBD_DEBUG 0
bool debug_keyboard = false;

guint Keyboard::edit_but = 3;
guint Keyboard::edit_mod = GDK_CONTROL_MASK;
guint Keyboard::delete_but = 3;
guint Keyboard::delete_mod = GDK_SHIFT_MASK;
guint Keyboard::snap_mod = GDK_MOD3_MASK;

#ifdef GTKOSX
guint Keyboard::PrimaryModifier = GDK_META_MASK;   // Command
guint Keyboard::SecondaryModifier = GDK_MOD1_MASK; // Alt/Option
guint Keyboard::TertiaryModifier = GDK_SHIFT_MASK; // Shift
guint Keyboard::CopyModifier = GDK_MOD1_MASK;      // Alt/Option
guint Keyboard::RangeSelectModifier = GDK_SHIFT_MASK;   
#else
guint Keyboard::PrimaryModifier = GDK_CONTROL_MASK; // Control
guint Keyboard::SecondaryModifier = GDK_MOD1_MASK;  // Alt/Option
guint Keyboard::TertiaryModifier = GDK_SHIFT_MASK;  // Shift
guint Keyboard::CopyModifier = GDK_CONTROL_MASK;    
guint Keyboard::RangeSelectModifier = GDK_SHIFT_MASK;   
#endif

Keyboard*    Keyboard::_the_keyboard = 0;
Gtk::Window* Keyboard::current_window = 0;
bool         Keyboard::_some_magic_widget_has_focus = false;

std::string Keyboard::user_keybindings_path;
bool Keyboard::can_save_keybindings = false;
map<string,string> Keyboard::binding_files;
std::string Keyboard::_current_binding_name = _("Unknown");

/* set this to initially contain the modifiers we care about, then track changes in ::set_edit_modifier() etc. */

GdkModifierType Keyboard::RelevantModifierKeyMask;

void
Keyboard::magic_widget_grab_focus () 
{
	_some_magic_widget_has_focus = true;
}

void
Keyboard::magic_widget_drop_focus ()
{
	_some_magic_widget_has_focus = false;
}

bool
Keyboard::some_magic_widget_has_focus ()
{
	return _some_magic_widget_has_focus;
}

Keyboard::Keyboard ()
{
	if (_the_keyboard == 0) {
		_the_keyboard = this;
	}

	RelevantModifierKeyMask = (GdkModifierType) gtk_accelerator_get_default_mod_mask ();

	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | PrimaryModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | SecondaryModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | TertiaryModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | CopyModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | RangeSelectModifier);

	snooper_id = gtk_key_snooper_install (_snooper, (gpointer) this);

	XMLNode* node = ARDOUR_UI::instance()->keyboard_settings();
	set_state (*node);
}

Keyboard::~Keyboard ()
{
	gtk_key_snooper_remove (snooper_id);
}

XMLNode& 
Keyboard::get_state (void)
{
	XMLNode* node = new XMLNode ("Keyboard");
	char buf[32];

	snprintf (buf, sizeof (buf), "%d", edit_but);
	node->add_property ("edit-button", buf);
	snprintf (buf, sizeof (buf), "%d", edit_mod);
	node->add_property ("edit-modifier", buf);
	snprintf (buf, sizeof (buf), "%d", delete_but);
	node->add_property ("delete-button", buf);
	snprintf (buf, sizeof (buf), "%d", delete_mod);
	node->add_property ("delete-modifier", buf);
	snprintf (buf, sizeof (buf), "%d", snap_mod);
	node->add_property ("snap-modifier", buf);

	return *node;
}

int 
Keyboard::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("edit-button")) != 0) {
		sscanf (prop->value().c_str(), "%d", &edit_but);
	} 

	if ((prop = node.property ("edit-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &edit_mod);
	} 

	if ((prop = node.property ("delete-button")) != 0) {
		sscanf (prop->value().c_str(), "%d", &delete_but);
	} 

	if ((prop = node.property ("delete-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &delete_mod);
	} 

	if ((prop = node.property ("snap-modifier")) != 0) {
		sscanf (prop->value().c_str(), "%d", &snap_mod);
	} 

	return 0;
}

gint
Keyboard::_snooper (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	return ((Keyboard *) data)->snooper (widget, event);
}

gint
Keyboard::snooper (GtkWidget *widget, GdkEventKey *event)
{
	uint32_t keyval;

#if 0
	cerr << "snoop widget " << widget << " key " << event->keyval << " type: " << event->type 
	     << " state " << std::hex << event->state << std::dec
	     << endl;
#endif

#if KBD_DEBUG
	if (debug_keyboard) {
		cerr << "snoop widget " << widget << " key " << event->keyval << " type: " << event->type 
		     << endl;
	}
#endif

	if (event->keyval == GDK_Shift_R) {
		keyval = GDK_Shift_L;

	} else 	if (event->keyval == GDK_Control_R) {
		keyval = GDK_Control_L;

	} else {
		keyval = event->keyval;
	}
		
	if (event->type == GDK_KEY_PRESS) {

		if (find (state.begin(), state.end(), keyval) == state.end()) {
			state.push_back (keyval);
			sort (state.begin(), state.end());
		} 

	} else if (event->type == GDK_KEY_RELEASE) {

		State::iterator i;
		
		if ((i = find (state.begin(), state.end(), keyval)) != state.end()) {
			state.erase (i);
			sort (state.begin(), state.end());
		} 

	}

	if (event->type == GDK_KEY_RELEASE && event->keyval == GDK_w && modifier_state_equals (event->state, PrimaryModifier)) {
		if (current_window) {
			current_window->hide ();
			current_window = 0;
		}
	}

	return false;
}

bool
Keyboard::key_is_down (uint32_t keyval)
{
	return find (state.begin(), state.end(), keyval) != state.end();
}

bool
Keyboard::enter_window (GdkEventCrossing *ev, Gtk::Window* win)
{
	current_window = win;
	return false;
}

bool
Keyboard::leave_window (GdkEventCrossing *ev, Gtk::Window* win)
{
	switch (ev->detail) {
	case GDK_NOTIFY_INFERIOR:
		if (debug_keyboard) {
			cerr << "INFERIOR crossing ... out\n";
		}
		break;

	case GDK_NOTIFY_VIRTUAL:
		if (debug_keyboard) {
			cerr << "VIRTUAL crossing ... out\n";
		}
		/* fallthru */

	default:
		if (debug_keyboard) {
			cerr << "REAL CROSSING ... out\n";
			cerr << "clearing current target\n";
		}
		state.clear ();
		current_window = 0;
	}

	return false;
}

void
Keyboard::set_edit_button (guint but)
{
	edit_but = but;
}

void
Keyboard::set_edit_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~edit_mod);
	edit_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | edit_mod);
}

void
Keyboard::set_delete_button (guint but)
{
	delete_but = but;
}

void
Keyboard::set_delete_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~delete_mod);
	delete_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | delete_mod);
}

void
Keyboard::set_modifier (uint32_t newval, uint32_t& var)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~var);
	var = newval;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | var);
}

void
Keyboard::set_snap_modifier (guint mod)
{
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask & ~snap_mod);
	snap_mod = mod;
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | snap_mod);
}

bool
Keyboard::is_edit_event (GdkEventButton *ev)
{

	return (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_BUTTON_RELEASE) && 
		(ev->button == Keyboard::edit_button()) && 
		((ev->state & RelevantModifierKeyMask) == Keyboard::edit_modifier());
}

bool
Keyboard::is_delete_event (GdkEventButton *ev)
{
	return (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_BUTTON_RELEASE) && 
		(ev->button == Keyboard::delete_button()) && 
		((ev->state & RelevantModifierKeyMask) == Keyboard::delete_modifier());
}

bool
Keyboard::is_context_menu_event (GdkEventButton *ev)
{
	return (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_BUTTON_RELEASE) && 
		(ev->button == 3) && 
		((ev->state & RelevantModifierKeyMask) == 0);
}

bool 
Keyboard::no_modifiers_active (guint state)
{
	return (state & RelevantModifierKeyMask) == 0;
}

bool
Keyboard::modifier_state_contains (guint state, ModifierMask mask)
{
	return (state & mask) == (guint) mask;
}

bool
Keyboard::modifier_state_equals (guint state, ModifierMask mask)
{
	return (state & RelevantModifierKeyMask) == (guint) mask;
}

Selection::Operation
Keyboard::selection_type (guint state)
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


static void 
accel_map_changed (GtkAccelMap* map,
		   gchar* path,
		   guint  key,
		   GdkModifierType mod,
		   gpointer arg)
{
	Keyboard::save_keybindings ();
}

void
Keyboard::set_can_save_keybindings (bool yn)
{
	can_save_keybindings = yn;
}

void
Keyboard::save_keybindings ()
{
	if (can_save_keybindings) {
		Gtk::AccelMap::save (user_keybindings_path);
	} 
}

void
Keyboard::setup_keybindings ()
{
	using namespace ARDOUR_COMMAND_LINE;
	std::string default_bindings = "mnemonic-us.bindings";
	std::string path;
	vector<string> strs;

	ARDOUR::find_bindings_files (binding_files);

	/* set up the per-user bindings path */
	
	strs.push_back (Glib::get_home_dir());
	strs.push_back (".ardour2");
	strs.push_back ("ardour.bindings");

	user_keybindings_path = Glib::build_filename (strs);

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
			
			path = find_config_file (keybindings_path);
			
			if (path.empty()) {
				
				if (keybindings_path == default_bindings) {
					error << _("Default keybindings not found - Ardour will be hard to use!") << endmsg;
					return;
				} else {
					warning << string_compose (_("Key bindings file \"%1\" not found. Default bindings used instead"), 
								   keybindings_path)
						<< endmsg;
					keybindings_path = default_bindings;
				}

			} else {

				/* use it */

				keybindings_path = path;
				break;
				
			}

		} else {
			
			/* path is absolute already */

			if (!Glib::file_test (keybindings_path, Glib::FILE_TEST_EXISTS)) {
				if (keybindings_path == default_bindings) {
					error << _("Default keybindings not found - Ardour will be hard to use!") << endmsg;
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
	g_signal_connect (accelmap, "changed", (GCallback) accel_map_changed, 0);
}

bool
Keyboard::load_keybindings (string path)
{
	try {
		cerr << "loading bindings from " << path << endl;

		Gtk::AccelMap::load (path);

		_current_binding_name = _("Unknown");

		for (map<string,string>::iterator x = binding_files.begin(); x != binding_files.end(); ++x) {
			if (path == x->second) {
				_current_binding_name = x->first;
				break;
			}
		}

		return true;

	} catch (...) {
		error << string_compose (_("Ardour key bindings file not found at \"%1\" or contains errors."), path)
		      << endmsg;
		return false;
	}
}


