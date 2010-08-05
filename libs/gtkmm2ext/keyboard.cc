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

#include <vector>

#include <algorithm>
#include <fstream>
#include <iostream>

#include <ctype.h>

#include <gtkmm/widget.h>
#include <gtkmm/window.h>
#include <gtkmm/accelmap.h>
#include <gdk/gdkkeysyms.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/xml++.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/actions.h"

#include "i18n.h"

using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

#define KBD_DEBUG 1
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
guint Keyboard::Level4Modifier = GDK_CONTROL_MASK; // Control
guint Keyboard::CopyModifier = GDK_MOD1_MASK;      // Alt/Option
guint Keyboard::RangeSelectModifier = GDK_SHIFT_MASK;
guint Keyboard::button2_modifiers = Keyboard::SecondaryModifier|Keyboard::Level4Modifier;
#else
guint Keyboard::PrimaryModifier = GDK_CONTROL_MASK; // Control
guint Keyboard::SecondaryModifier = GDK_MOD1_MASK;  // Alt/Option
guint Keyboard::TertiaryModifier = GDK_SHIFT_MASK;  // Shift
guint Keyboard::Level4Modifier = GDK_MOD4_MASK;     // Mod4/Windows
guint Keyboard::CopyModifier = GDK_CONTROL_MASK;
guint Keyboard::RangeSelectModifier = GDK_SHIFT_MASK;
guint Keyboard::button2_modifiers = 0; /* not used */
#endif


Keyboard*    Keyboard::_the_keyboard = 0;
Gtk::Window* Keyboard::current_window = 0;
bool         Keyboard::_some_magic_widget_has_focus = false;

std::string Keyboard::user_keybindings_path;
bool Keyboard::can_save_keybindings = false;
bool Keyboard::bindings_changed_after_save_became_legal = false;
map<string,string> Keyboard::binding_files;
string Keyboard::_current_binding_name;
map<AccelKey,pair<string,string>,Keyboard::AccelKeyLess> Keyboard::release_keys;

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
                _current_binding_name = _("Unknown");
	}

	RelevantModifierKeyMask = (GdkModifierType) gtk_accelerator_get_default_mod_mask ();

	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | PrimaryModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | SecondaryModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | TertiaryModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | Level4Modifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | CopyModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | RangeSelectModifier);

	gtk_accelerator_set_default_mod_mask (RelevantModifierKeyMask);

	snooper_id = gtk_key_snooper_install (_snooper, (gpointer) this);
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
Keyboard::set_state (const XMLNode& node, int /*version*/)
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
	bool ret = false;

#if 0
	cerr << "snoop widget " << widget << " key " << event->keyval << " type: " << event->type
	     << " state " << std::hex << event->state << std::dec
             << " magic ? " << _some_magic_widget_has_focus 
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

		} else {

			/* key is already down. if its also used for release,
			   prevent auto-repeat events.
			*/

			for (map<AccelKey,two_strings,AccelKeyLess>::iterator k = release_keys.begin(); k != release_keys.end(); ++k) {

				const AccelKey& ak (k->first);

				if (keyval == ak.get_key() && (Gdk::ModifierType)((event->state & Keyboard::RelevantModifierKeyMask) | Gdk::RELEASE_MASK) == ak.get_mod()) {
					cerr << "Suppress auto repeat\n";
					ret = true;
					break;
				}
			}
		}

	} else if (event->type == GDK_KEY_RELEASE) {

		State::iterator i;

		if ((i = find (state.begin(), state.end(), keyval)) != state.end()) {
			state.erase (i);
			sort (state.begin(), state.end());
		}

		for (map<AccelKey,two_strings,AccelKeyLess>::iterator k = release_keys.begin(); k != release_keys.end(); ++k) {

			const AccelKey& ak (k->first);
			two_strings ts (k->second);

			if (keyval == ak.get_key() && (Gdk::ModifierType)((event->state & Keyboard::RelevantModifierKeyMask) | Gdk::RELEASE_MASK) == ak.get_mod()) {
				Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (ts.first.c_str(), ts.second.c_str());
				if (act) {
					act->activate();
					cerr << "use repeat, suppress other\n";
					ret = true;
				}
				break;
			}
		}
	}

	/* Special keys that we want to handle in
	   any dialog, no matter whether it uses
	   the regular set of accelerators or not
	*/

	if (event->type == GDK_KEY_RELEASE && modifier_state_equals (event->state, PrimaryModifier)) {
		switch (event->keyval) {
		case GDK_w:
			if (current_window) {
				current_window->hide ();
				current_window = 0;
				ret = true;
			}
			break;
		}
	}

	return ret;
}

bool
Keyboard::key_is_down (uint32_t keyval)
{
	return find (state.begin(), state.end(), keyval) != state.end();
}

bool
Keyboard::enter_window (GdkEventCrossing *, Gtk::Window* win)
{
	current_window = win;
	return false;
}

bool
Keyboard::leave_window (GdkEventCrossing *ev, Gtk::Window* /*win*/)
{
	if (ev) {
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
	} else {
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
Keyboard::is_button2_event (GdkEventButton* ev)
{
#ifdef GTKOSX
	return (ev->button == 2) ||
		((ev->button == 1) &&
		 ((ev->state & Keyboard::button2_modifiers) == Keyboard::button2_modifiers));
#else
	return ev->button == 2;
#endif
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

void
Keyboard::keybindings_changed ()
{
	if (Keyboard::can_save_keybindings) {
		Keyboard::bindings_changed_after_save_became_legal = true;
	}

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
	if (can_save_keybindings && bindings_changed_after_save_became_legal) {
		Gtk::AccelMap::save (user_keybindings_path);
	}
}

bool
Keyboard::load_keybindings (string path)
{
	try {
		info << "Loading bindings from " << path << endl;

		Gtk::AccelMap::load (path);

		_current_binding_name = _("Unknown");

		for (map<string,string>::iterator x = binding_files.begin(); x != binding_files.end(); ++x) {
			if (path == x->second) {
				_current_binding_name = x->first;
				break;
			}
		}


	} catch (...) {
		error << string_compose (_("Ardour key bindings file not found at \"%1\" or contains errors."), path)
		      << endmsg;
		return false;
	}

	/* now find all release-driven bindings */

	vector<string> groups;
	vector<string> names;
	vector<AccelKey> bindings;

	ActionManager::get_all_actions (groups, names, bindings);

	vector<string>::iterator g;
	vector<AccelKey>::iterator b;
	vector<string>::iterator n;

	release_keys.clear ();

	bool show_bindings = (getenv ("ARDOUR_SHOW_BINDINGS") != 0);

	for (n = names.begin(), b = bindings.begin(), g = groups.begin(); n != names.end(); ++n, ++b, ++g) {

		if (show_bindings) {

			cerr << "Action: " << (*n) << " Group: " << (*g) << " binding = ";

			if ((*b).get_key() != GDK_VoidSymbol) {
				cerr << (*b).get_key() << " w/mod = " << hex << (*b).get_mod() << dec << " = " << (*b).get_abbrev();
			} else {
				cerr << "unbound";
			}

			cerr << endl;
		}
	}

	for (n = names.begin(), b = bindings.begin(), g = groups.begin(); n != names.end(); ++n, ++b, ++g) {
		if ((*b).get_mod() & Gdk::RELEASE_MASK) {
			release_keys.insert (pair<AccelKey,two_strings> (*b, two_strings (*g, *n)));
		}
	}

	return true;
}


