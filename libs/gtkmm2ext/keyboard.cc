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
#include <cerrno>
#include <ctype.h>

#include "pbd/gstdio_compat.h"

#include <gtkmm/widget.h>
#include <gtkmm/window.h>
#include <gtkmm/accelmap.h>
#include <gdk/gdkkeysyms.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/xml++.h"
#include "pbd/debug.h"
#include "pbd/unwind.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/debug.h"
#include "gtkmm2ext/utils.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

guint Keyboard::edit_but = 3;
guint Keyboard::edit_mod = GDK_CONTROL_MASK;
guint Keyboard::delete_but = 3;
guint Keyboard::delete_mod = GDK_SHIFT_MASK;
guint Keyboard::insert_note_but = 1;
guint Keyboard::insert_note_mod = GDK_CONTROL_MASK;

#ifdef __APPLE__

guint Keyboard::PrimaryModifier = GDK_MOD2_MASK;   // Command
guint Keyboard::SecondaryModifier = GDK_CONTROL_MASK; // Control
guint Keyboard::TertiaryModifier = GDK_SHIFT_MASK; // Shift
guint Keyboard::Level4Modifier = GDK_MOD1_MASK; // Alt/Option
guint Keyboard::CopyModifier = GDK_CONTROL_MASK;      // Control
guint Keyboard::RangeSelectModifier = GDK_SHIFT_MASK;
guint Keyboard::button2_modifiers = Keyboard::SecondaryModifier|Keyboard::Level4Modifier;

const char* Keyboard::primary_modifier_name() { return _("Command"); }
const char* Keyboard::secondary_modifier_name() { return _("Control"); }
const char* Keyboard::tertiary_modifier_name() { return S_("Key|Shift"); }
const char* Keyboard::level4_modifier_name() { return _("Option"); }

const char* Keyboard::primary_modifier_short_name() { return _("Cmd"); }
const char* Keyboard::secondary_modifier_short_name() { return _("Ctrl"); }
const char* Keyboard::tertiary_modifier_short_name() { return S_("Key|Shift"); }
const char* Keyboard::level4_modifier_short_name() { return _("Opt"); }

guint Keyboard::snap_mod = Keyboard::Level4Modifier|Keyboard::TertiaryModifier; // XXX this is probably completely wrong
guint Keyboard::snap_delta_mod = Keyboard::Level4Modifier;

#else

guint Keyboard::PrimaryModifier = GDK_CONTROL_MASK; // Control
guint Keyboard::SecondaryModifier = GDK_MOD1_MASK;  // Alt/Option
guint Keyboard::TertiaryModifier = GDK_SHIFT_MASK;  // Shift
guint Keyboard::Level4Modifier = GDK_MOD4_MASK|GDK_SUPER_MASK; // Mod4/Windows
guint Keyboard::CopyModifier = GDK_CONTROL_MASK;
guint Keyboard::RangeSelectModifier = GDK_SHIFT_MASK;
guint Keyboard::button2_modifiers = 0; /* not used */

const char* Keyboard::primary_modifier_name() { return _("Control"); }
const char* Keyboard::secondary_modifier_name() { return _("Alt"); }
const char* Keyboard::tertiary_modifier_name() { return S_("Key|Shift"); }
const char* Keyboard::level4_modifier_name() { return _("Windows"); }

const char* Keyboard::primary_modifier_short_name() { return _("Ctrl"); }
const char* Keyboard::secondary_modifier_short_name() { return _("Alt"); }
const char* Keyboard::tertiary_modifier_short_name() { return S_("Key|Shift"); }
const char* Keyboard::level4_modifier_short_name() { return _("Win"); }

guint Keyboard::snap_mod = Keyboard::SecondaryModifier;
guint Keyboard::snap_delta_mod = Keyboard::SecondaryModifier|Keyboard::Level4Modifier;

#endif

guint Keyboard::GainFineScaleModifier = Keyboard::PrimaryModifier;
guint Keyboard::GainExtraFineScaleModifier = Keyboard::SecondaryModifier;

guint Keyboard::ScrollZoomVerticalModifier = Keyboard::SecondaryModifier;
guint Keyboard::ScrollZoomHorizontalModifier = Keyboard::PrimaryModifier;
guint Keyboard::ScrollHorizontalModifier = Keyboard::TertiaryModifier;

Keyboard*    Keyboard::_the_keyboard = 0;
Gtk::Window* Keyboard::current_window = 0;
bool         Keyboard::_some_magic_widget_has_focus = false;

std::string Keyboard::user_keybindings_path;
bool Keyboard::can_save_keybindings = false;
bool Keyboard::bindings_changed_after_save_became_legal = false;
map<string,string> Keyboard::binding_files;
string Keyboard::_current_binding_name;
string Keyboard::binding_filename_suffix = X_(".keys");
Gtk::Window* Keyboard::pre_dialog_active_window = 0;

/* set this to initially contain the modifiers we care about, then track changes in ::set_edit_modifier() etc. */
GdkModifierType Keyboard::RelevantModifierKeyMask;
sigc::signal0<void> Keyboard::RelevantModifierKeysChanged;

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

	reset_relevant_modifier_key_mask();

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

	node->set_property ("copy-modifier", CopyModifier);
	node->set_property ("edit-button", edit_but);
	node->set_property ("edit-modifier", edit_mod);
	node->set_property ("delete-button", delete_but);
	node->set_property ("delete-modifier", delete_mod);
	node->set_property ("snap-modifier", snap_mod);
	node->set_property ("snap-delta-modifier", snap_delta_mod);
	node->set_property ("insert-note-button", insert_note_but);
	node->set_property ("insert-note-modifier", insert_note_mod);

	return *node;
}

int
Keyboard::set_state (const XMLNode& node, int /*version*/)
{
	node.get_property ("copy-modifier", CopyModifier);
	node.get_property ("edit-button", edit_but);
	node.get_property ("edit-modifier", edit_mod);
	node.get_property ("delete-button", delete_but);
	node.get_property ("delete-modifier", delete_mod);
	node.get_property ("snap-modifier", snap_mod);
	node.get_property ("snap-delta-modifier", snap_delta_mod);
	node.get_property ("insert-note-button", insert_note_but);
	node.get_property ("insert-note-modifier", insert_note_mod);

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

	DEBUG_TRACE (
		DEBUG::Keyboard,
		string_compose (
			"Snoop widget %1 name: [%6] key %2 [%8] type %3 state %4 [%7] magic %5\n",
			widget, event->keyval, event->type, event->state, _some_magic_widget_has_focus,
			gtk_widget_get_name (widget), show_gdk_event_state (event->state), gdk_keyval_name (event->keyval)
			)
		);

	if (event->keyval == GDK_Shift_R) {
		keyval = GDK_Shift_L;

	} else if (event->keyval == GDK_Control_R) {
		keyval = GDK_Control_L;

	} else {
		keyval = event->keyval;
	}

	if (event->state & ScrollZoomVerticalModifier) {
		/* There is a special and rather hacky situation in Editor which makes
		   it useful to know when the modifier key for vertical zoom has been
		   released, so emit a signal here (see Editor::_stepping_axis_view).
		   Note that the state bit for the modifier key is set for the key-up
		   event when the modifier is released, but not the key-down when it
		   is pressed, so we get here on key-up, which is what we want.
		*/
		ZoomVerticalModifierReleased (); /* EMIT SIGNAL */
	}

	if (event->type == GDK_KEY_PRESS) {

		if (find (state.begin(), state.end(), keyval) == state.end()) {
			state.push_back (keyval);
			sort (state.begin(), state.end());

		} else {

			/* key is already down. if its also used for release,
			   prevent auto-repeat events.
			*/

#if 0
			/* August 2015: we don't have any release bindings
			 */

			for (map<AccelKey,two_strings,AccelKeyLess>::iterator k = release_keys.begin(); k != release_keys.end(); ++k) {

				const AccelKey& ak (k->first);

				if (keyval == ak.get_key() && (Gdk::ModifierType)((event->state & Keyboard::RelevantModifierKeyMask) | Gdk::RELEASE_MASK) == ak.get_mod()) {
					DEBUG_TRACE (DEBUG::Keyboard, "Suppress auto repeat\n");
					ret = true;
					break;
				}
			}
#endif
		}
	}

	if (event->type == GDK_KEY_RELEASE) {

		State::iterator k = find (state.begin(), state.end(), keyval);

		if (k != state.end()) {
			/* this cannot change the ordering, so need to sort */
			state.erase (k);
			if (state.empty()) {
				DEBUG_TRACE (DEBUG::Keyboard, "no keys down\n");
			} else {
#ifndef NDEBUG
				if (DEBUG_ENABLED(DEBUG::Keyboard)) {
					DEBUG_STR_DECL(a);
					DEBUG_STR_APPEND(a, "keyboard, keys still down: ");
					for (State::iterator i = state.begin(); i != state.end(); ++i) {
						DEBUG_STR_APPEND(a, gdk_keyval_name (*i));
						DEBUG_STR_APPEND(a, ',');
					}
					DEBUG_STR_APPEND(a, '\n');
					DEBUG_TRACE (DEBUG::Keyboard, DEBUG_STR(a).str());
				}
#endif /* NDEBUG */
			}
		}

		if (modifier_state_equals (event->state, PrimaryModifier)) {

			/* Special keys that we want to handle in
			   any dialog, no matter whether it uses
			   the regular set of accelerators or not
			*/

			switch (event->keyval) {
			case GDK_w:
				close_current_dialog ();
				ret = true;
				break;
			}
		}
	}

	DEBUG_TRACE (DEBUG::Keyboard, string_compose ("snooper returns %1\n", ret));

	return ret;
}

void
Keyboard::reset_relevant_modifier_key_mask ()
{
	RelevantModifierKeyMask = (GdkModifierType) gtk_accelerator_get_default_mod_mask ();

	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | PrimaryModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | SecondaryModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | TertiaryModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | Level4Modifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | CopyModifier);
	RelevantModifierKeyMask = GdkModifierType (RelevantModifierKeyMask | RangeSelectModifier);

	gtk_accelerator_set_default_mod_mask (RelevantModifierKeyMask);

	RelevantModifierKeysChanged(); /* EMIT SIGNAL */
}

void
Keyboard::close_current_dialog ()
{
	if (current_window) {
		current_window->hide ();
		current_window = 0;

                if (pre_dialog_active_window) {
                        pre_dialog_active_window->present ();
                        pre_dialog_active_window = 0;
                }
	}
}

bool
Keyboard::catch_user_event_for_pre_dialog_focus (GdkEvent* ev, Gtk::Window* w)
{
        switch (ev->type) {
        case GDK_BUTTON_PRESS:
        case GDK_BUTTON_RELEASE:
        case GDK_KEY_PRESS:
        case GDK_KEY_RELEASE:
                pre_dialog_active_window = w;
                break;

        case GDK_FOCUS_CHANGE:
                if (ev->focus_change.in) {
                        pre_dialog_active_window = w;
                }
                break;

        default:
                break;
        }
        return false;
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
	DEBUG_TRACE (DEBUG::Keyboard, string_compose ("Entering window, title = %1\n", win->get_title()));
	return false;
}

bool
Keyboard::leave_window (GdkEventCrossing *ev, Gtk::Window* /*win*/)
{
	if (ev) {
		switch (ev->detail) {
		case GDK_NOTIFY_INFERIOR:
			DEBUG_TRACE (DEBUG::Keyboard, "INFERIOR crossing ... out\n");
			break;

		case GDK_NOTIFY_VIRTUAL:
			DEBUG_TRACE (DEBUG::Keyboard, "VIRTUAL crossing ... out\n");
			/* fallthru */

		default:
			DEBUG_TRACE (DEBUG::Keyboard, "REAL crossing ... out\n");
			DEBUG_TRACE (DEBUG::Keyboard, "Clearing current target\n");
			state.clear ();
			current_window = 0;
		}
	} else {
		DEBUG_TRACE (DEBUG::Keyboard, "LEAVE window without event\n");
		current_window = 0;
	}

	return false;
}

bool
Keyboard::focus_in_window (GdkEventFocus *, Gtk::Window* win)
{
	current_window = win;
	DEBUG_TRACE (DEBUG::Keyboard, string_compose ("Focusing in window, title = %1\n", win->get_title()));
	return false;
}

bool
Keyboard::focus_out_window (GdkEventFocus * ev, Gtk::Window* win)
{
	if (ev) {
		state.clear ();
		current_window = 0;
	}  else {
		if (win == current_window) {
			current_window = 0;
		}
	}

	DEBUG_TRACE (DEBUG::Keyboard, string_compose ("Foucusing out window, title = %1\n", win->get_title()));

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
	edit_mod = mod;
	reset_relevant_modifier_key_mask();
}

void
Keyboard::set_delete_button (guint but)
{
	delete_but = but;
}

void
Keyboard::set_delete_modifier (guint mod)
{
	delete_mod = mod;
	reset_relevant_modifier_key_mask();
}

void
Keyboard::set_insert_note_button (guint but)
{
	insert_note_but = but;
}

void
Keyboard::set_insert_note_modifier (guint mod)
{
	insert_note_mod = mod;
	reset_relevant_modifier_key_mask();
}


void
Keyboard::set_modifier (uint32_t newval, uint32_t& var)
{
	var = newval;
	reset_relevant_modifier_key_mask();
}

void
Keyboard::set_snap_modifier (guint mod)
{
	snap_mod = mod;
	reset_relevant_modifier_key_mask();
}

void
Keyboard::set_snap_delta_modifier (guint mod)
{
	snap_delta_mod = mod;
	reset_relevant_modifier_key_mask();
}

bool
Keyboard::is_edit_event (GdkEventButton *ev)
{
	return (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_BUTTON_RELEASE) &&
		(ev->button == Keyboard::edit_button()) &&
		((ev->state & RelevantModifierKeyMask) == Keyboard::edit_modifier());
}

bool
Keyboard::is_insert_note_event (GdkEventButton *ev)
{
	return (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_BUTTON_RELEASE) &&
		(ev->button == Keyboard::insert_note_button()) &&
		((ev->state & RelevantModifierKeyMask) == Keyboard::insert_note_modifier());
}

bool
Keyboard::is_button2_event (GdkEventButton* ev)
{
#ifdef __APPLE__
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
		/* Call to specific implementation to save bindings to path */
		store_keybindings (user_keybindings_path);
	}
}

bool
Keyboard::load_keybindings (string const & path)
{
	try {
		info << "Loading bindings from " << path << endl;

		/* Call to specific implementation to load bindings from path */
		read_keybindings (path);

		_current_binding_name = _("Unknown");

		for (map<string,string>::iterator x = binding_files.begin(); x != binding_files.end(); ++x) {
			if (path == x->second) {
				_current_binding_name = x->first;
				break;
			}
		}


	} catch (...) {
		error << string_compose (_("key bindings file not found at \"%2\" or contains errors."), path)
		      << endmsg;
		return false;
	}

	return true;
}

int
Keyboard::read_keybindings (string const & path)
{
	XMLTree tree;

	if (!tree.read (path.c_str())) {
		return -1;
	}

	/* toplevel node is "BindingSet; children are "Bindings" */

	XMLNodeList const& children = tree.root()->children();

	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		XMLNode const * child = *i;
		if (child->name() == X_("Bindings")) {
		        XMLProperty const* name = child->property (X_("name"));
		        if (!name) {
			        warning << _("Keyboard binding found without a name") << endmsg;
			        continue;
		        }

		        Bindings* b = new Bindings (name->value());
		        b->load (**i);
	        }
        }

	return 0;
}

int
Keyboard::store_keybindings (string const & path)
{
	XMLNode* node = new XMLNode (X_("BindingSet"));
	XMLNode* bnode;
	int ret = 0;

	for (list<Bindings*>::const_iterator b = Bindings::bindings.begin(); b != Bindings::bindings.end(); ++b) {
		bnode = new XMLNode (X_("Bindings"));
		bnode->set_property (X_("name"), (*b)->name());
		(*b)->save (*bnode);
		node->add_child_nocopy (*bnode);
	}

	XMLTree tree;
	tree.set_root (node); /* tree now owns root and will delete it */

	if (!tree.write (path)) {
		error << string_compose (_("Cannot save key bindings to %1"), path) << endmsg;
		ret = -1;
	}

	return ret;
}

int
Keyboard::reset_bindings ()
{
	if (Glib::file_test (user_keybindings_path,  Glib::FILE_TEST_EXISTS)) {

		string new_path = user_keybindings_path;
		new_path += ".old";

		if (::g_rename (user_keybindings_path.c_str(), new_path.c_str())) {
			error << string_compose (_("Cannot rename your own keybinding file (%1)"), strerror (errno)) << endmsg;
			return -1;
		}
	}

	{
		PBD::Unwinder<bool> uw (can_save_keybindings, false);
		Bindings::reset_bindings ();
		setup_keybindings ();
		Bindings::associate_all ();
	}

	return 0;
}
