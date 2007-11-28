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

#include "ardour_ui.h"

#include <algorithm>
#include <fstream>

#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <pbd/error.h>

#include "keyboard.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace PBD;

#define KBD_DEBUG 0
bool debug_keyboard = false;

guint Keyboard::edit_but = 3;
guint Keyboard::edit_mod = GDK_CONTROL_MASK;
guint Keyboard::delete_but = 3;
guint Keyboard::delete_mod = GDK_SHIFT_MASK;
guint Keyboard::snap_mod = GDK_MOD3_MASK;

#ifdef NATIVE_OSX_KEYS
guint Keyboard::PrimaryModifier = GDK_MOD1_MASK;   // Command
guint Keyboard::SecondaryModifier = GDK_MOD5_MASK; // Alt/Option
guint Keyboard::TertiaryModifier = GDK_SHIFT_MASK; // Shift
guint Keyboard::CopyModifier = GDK_MOD5_MASK;      // Alt/Option
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
