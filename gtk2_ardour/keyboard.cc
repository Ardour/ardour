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

    $Id$
*/

#include "ardour_ui.h"

#include <algorithm>
#include <fstream>

#include <ctype.h>

#include <X11/keysymdef.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <pbd/error.h>

#include "keyboard.h"
#include "keyboard_target.h"
#include "ardour_dialog.h"
#include "gui_thread.h"

#include "i18n.h"

#define KBD_DEBUG 1
bool debug_keyboard = false;

guint Keyboard::edit_but = 3;
guint Keyboard::edit_mod = GDK_CONTROL_MASK;
guint Keyboard::delete_but = 3;
guint Keyboard::delete_mod = GDK_SHIFT_MASK;
guint Keyboard::snap_mod = GDK_MOD3_MASK;

uint32_t Keyboard::Control = GDK_CONTROL_MASK;
uint32_t Keyboard::Shift = GDK_SHIFT_MASK;
uint32_t Keyboard::Alt = GDK_MOD1_MASK;
uint32_t Keyboard::Meta = GDK_MOD2_MASK;

Keyboard* Keyboard::_the_keyboard = 0;

/* set this to initially contain the modifiers we care about, then track changes in ::set_edit_modifier() etc. */

GdkModifierType Keyboard::RelevantModifierKeyMask = 
                               GdkModifierType (GDK_SHIFT_MASK|GDK_CONTROL_MASK|GDK_MOD3_MASK);


Keyboard::Keyboard ()
{
	if (_the_keyboard == 0) {
		_the_keyboard = this;
	}

	target = 0;
	default_target = 0;
	_queue_events = false;
	_flush_queue = false;
	playback_ignore_count = 0;
	focus_allowed = false;
	collecting_prefix = false;
	current_dialog = 0;

	get_modifier_masks ();

	snooper_id = gtk_key_snooper_install (_snooper, (gpointer) this);

	/* some global key actions */

	KeyboardTarget::add_action ("close-dialog", mem_fun(*this, &Keyboard::close_current_dialog));

	XMLNode* node = ARDOUR_UI::instance()->keyboard_settings();
	set_state (*node);
}

Keyboard::~Keyboard ()
{
	gtk_key_snooper_remove (snooper_id);
	delete [] modifier_masks;
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
	bool handled = false;
	uint32_t keyval;

#if KBD_DEBUG
	if (debug_keyboard) {
		cerr << "snoop widget " << widget << " key " << event->keyval << " type: " << event->type 
		     << " focus allowed? " << focus_allowed << " current dialog = " << current_dialog
		     << endl;
	}
#endif

	/* Only allow key events to propagate to the
	   usual GTK model when specifically allowed. 
	   Returning FALSE here does that.
	*/

	if (focus_allowed) {
		return FALSE;
	}
	
	if (event->keyval == GDK_Shift_R) {
		keyval = GDK_Shift_L;

	} else 	if (event->keyval == GDK_Control_R) {
		keyval = GDK_Control_L;

	} else {
		keyval = event->keyval;
	}
	
		
	if (event->type == GDK_KEY_PRESS) {
		bool was_prefix = false;

		if (collecting_prefix) {
			switch (keyval) {
			case GDK_0:
				current_prefix += '0';
				was_prefix = true;
				break;
			case GDK_1:
				current_prefix += '1';
				was_prefix = true;
				break;
			case GDK_2:
				current_prefix += '2';
				was_prefix = true;
				break;
			case GDK_3:
				current_prefix += '3';
				was_prefix = true;
				break;
			case GDK_4:
				current_prefix += '4';
				was_prefix = true;
				break;
			case GDK_5:
				current_prefix += '5';
				was_prefix = true;
				break;
			case GDK_6:
				current_prefix += '6';
				was_prefix = true;
				break;
			case GDK_7:
				current_prefix += '7';
				was_prefix = true;
				break;
			case GDK_8:
				current_prefix += '8';
				was_prefix = true;
				break;
			case GDK_9:
				current_prefix += '9';
				was_prefix = true;
				break;
			case GDK_period:
				current_prefix += '.';
				was_prefix = true;
				break;
			default:
				was_prefix = false;
				collecting_prefix = false;
				break;
			}
		}

		if (find (state.begin(), state.end(), keyval) == state.end()) {
			state.push_back (keyval);
			sort (state.begin(), state.end());
		}

#if KBD_DEBUG
		if (debug_keyboard) {
			cerr << "STATE: ";
			for (State::iterator i = state.begin(); i != state.end(); ++i) {
				cerr << (*i) << ' ';
			}
			cerr << endl;
		}
#endif

		if (!was_prefix) {

			bool old_collecting_prefix = collecting_prefix;

			if (target) {
#if KBD_DEBUG
				if (debug_keyboard) {
					cerr << "PRESS: delivering to target " << target << endl;
				}
#endif
				target->key_press_event (event, state, handled);
			}
			
			if (!handled && default_target) {
#if KBD_DEBUG
				if (debug_keyboard) {
					cerr << "PRESS: not handled, delivering to default target " << default_target << endl;
				}
#endif
				default_target->key_press_event (event, state, handled);
			}

#if KBD_DEBUG
			if (debug_keyboard) {
				cerr << "PRESS: handled ? " << handled << endl;
			}
#endif

			if (handled) {
				
				/* don't reset collecting prefix is start_prefix()
				   was called by the handler.
				*/
				
				if (collecting_prefix == old_collecting_prefix) {
					collecting_prefix = false;
					current_prefix = "";
				}
			}
		}

	} else if (event->type == GDK_KEY_RELEASE) {

		State::iterator i;
		
		if ((i = find (state.begin(), state.end(), keyval)) != state.end()) {
			state.erase (i);
			sort (state.begin(), state.end());
		} 

		if (target) {
#if KBD_DEBUG
			if (debug_keyboard) {
				cerr << "RELEASE: delivering to target " << target << endl;
			}
#endif
			target->key_release_event (event, state);
		} 

		if (default_target) {
#if KBD_DEBUG
			if (debug_keyboard) {
				cerr << "RELEASE: delivering to default target " << default_target << endl;
			}
#endif
			default_target->key_release_event (event, state);
		}
	}

	return TRUE;
}

bool
Keyboard::key_is_down (uint32_t keyval)
{
	return find (state.begin(), state.end(), keyval) != state.end();
}

void
Keyboard::set_target (KeyboardTarget *kt)
{
	/* XXX possible thread issues here */
	target = kt;
}

void
Keyboard::maybe_unset_target (KeyboardTarget* kt)
{
	if (target == kt) {
		target = 0;
	}
}

void
Keyboard::set_default_target (KeyboardTarget *kt)
{
	/* XXX possible thread issues here */

	default_target = kt;
}

void
Keyboard::allow_focus (bool yn)
{
	focus_allowed = yn;
}

Keyboard::State
Keyboard::translate_key_name (const string& name)

{
	string::size_type i;
	string::size_type len;
	bool at_end;
	string::size_type hyphen;
	string keyname;
	string whatevers_left;
	State result;
	guint keycode;
	
	i = 0;
	len = name.length();
	at_end = (len == 0);

	while (!at_end) {

		whatevers_left = name.substr (i);

		if ((hyphen = whatevers_left.find_first_of ('-')) == string::npos) {
			
                        /* no hyphen, so use the whole thing */
			
			keyname = whatevers_left;
			at_end = true;

		} else {

			/* There is a hyphen. */
			
			if (hyphen == 0 && whatevers_left.length() == 1) {
				/* its the first and only character */
			
				keyname = "-";
				at_end = true;

			} else {

				/* use the text before the hypen */
				
				keyname = whatevers_left.substr (0, hyphen);
				
				if (hyphen == len - 1) {
					at_end = true;
				} else {
					i += hyphen + 1;
					at_end = (i >= len);
				}
			}
		}
		
		if (keyname.length() == 1 && isupper (keyname[0])) {
			result.push_back (GDK_Shift_L);
		}
		
		if ((keycode = gdk_keyval_from_name(get_real_keyname (keyname).c_str())) == GDK_VoidSymbol) {
			error << compose(_("KeyboardTarget: keyname \"%1\" is unknown."), keyname) << endmsg;
			result.clear();
			return result;
		}
		
		result.push_back (keycode);
	}

	sort (result.begin(), result.end());

	return result;
}

string
Keyboard::get_real_keyname (const string& name)
{

	if (name == "Control" || name == "Ctrl") {
		return "Control_L";
	} 
	if (name == "Meta" || name == "MetaL") {
		return "Meta_L";
	} 
	if (name == "MetaR") {
		return "Meta_R";
	} 
	if (name == "Alt" || name == "AltL") {
		return "Alt_L";
	} 
	if (name == "AltR") {
		return "Alt_R";
	} 
	if (name == "Shift") {
		return "Shift_L";
	}
	if (name == "Shift_R") {
		return "Shift_L";
	}
	if (name == " ") {
		return "space";
	}
	if (name == "!") {
		return "exclam";
	}
	if (name == "\"") {
		return "quotedbl";
	}
	if (name == "#") {
		return "numbersign";
	}
	if (name == "$") {
		return "dollar";
	}
	if (name == "%") {
		return "percent";
	}
	if (name == "&") {
		return "ampersand";
	}
	if (name == "'") {
		return "apostrophe";
	}
	if (name == "'") {
		return "quoteright";
	}
	if (name == "(") {
		return "parenleft";
	}
	if (name == ")") {
		return "parenright";
	}
	if (name == "*") {
		return "asterisk";
	}
	if (name == "+") {
		return "plus";
	}
	if (name == ",") {
		return "comma";
	}
	if (name == "-") {
		return "minus";
	}
	if (name == ".") {
		return "period";
	}
	if (name == "/") {
		return "slash";
	}
	if (name == ":") {
		return "colon";
	}
	if (name == ";") {
		return "semicolon";
	}
	if (name == "<") {
		return "less";
	}
	if (name == "=") {
		return "equal";
	}
	if (name == ">") {
		return "greater";
	}
	if (name == "?") {
		return "question";
	}
	if (name == "@") {
		return "at";
	}
	if (name == "[") {
		return "bracketleft";
	}
	if (name == "\\") {
		return "backslash";
	}
	if (name == "]") {
		return "bracketright";
	}
	if (name == "^") {
		return "asciicircum";
	}
	if (name == "_") {
		return "underscore";
	}
	if (name == "`") {
		return "grave";
	}
	if (name == "`") {
		return "quoteleft";
	}
	if (name == "{") {
		return "braceleft";
	}
	if (name == "|") {
		return "bar";
	}
	if (name == "}") {
		return "braceright";
	}
	if (name == "~") {
		return "asciitilde";
	}

	return name;
}

int
Keyboard::get_prefix (float& val, bool& was_floating)
{
	if (current_prefix.length()) {
		if (current_prefix.find ('.') != string::npos) {
			was_floating = true;
		} else {
			was_floating = false;
		}
		if (sscanf (current_prefix.c_str(), "%f", &val) == 1) {
			return 0;
		}
		current_prefix = "";
	}
	return -1;
}

void
Keyboard::start_prefix ()
{
	collecting_prefix = true;
	current_prefix = "";
}

void
Keyboard::clear_modifier_state ()
{
	modifier_mask = 0;
}

void
Keyboard::check_modifier_state ()
{
	char keys[32];
	int i, j;

	clear_modifier_state ();
	XQueryKeymap (GDK_DISPLAY(), keys);

	for (i = 0; i < 32; ++i) {
		for (j = 0; j < 8; ++j) {

			if (keys[i] & (1<<j)) {
				modifier_mask |= modifier_masks[(i*8)+j];
			}
		}
	}
}

void
Keyboard::check_meta_numlock (char keycode, guint mod, string modname)
{
	guint alternate_meta_mod;
	string alternate_meta_modname;

	if (mod == Meta) {
		
		guint keysym = XKeycodeToKeysym  (GDK_DISPLAY(), keycode, 0);
		
		if (keysym == GDK_Num_Lock) {

			switch (mod) {
			case GDK_MOD2_MASK:
				alternate_meta_mod = GDK_MOD3_MASK;
				alternate_meta_modname = "Mod3";
				break;
			case GDK_MOD3_MASK:
				alternate_meta_mod = GDK_MOD2_MASK;
				alternate_meta_modname = "Mod2";
				break;
			case GDK_MOD4_MASK:
				alternate_meta_mod = GDK_MOD2_MASK;
				alternate_meta_modname = "Mod2";
				break;
			case GDK_MOD5_MASK:
				alternate_meta_mod = GDK_MOD2_MASK;
				alternate_meta_modname = "Mod2";
				break;
			default:
				error << compose (_("Your system is completely broken - NumLock uses \"%1\""
						    "as its modifier. This is madness - see the man page "
						    "for xmodmap to find out how to fix this."),
						  modname)
				      << endmsg;
				return;
			}

			warning << compose (_("Your system generates \"%1\" when the NumLock key "
					      "is pressed. This can cause problems when editing "
					      "so Ardour will use %2 to mean Meta rather than %1"),
					    modname, alternate_meta_modname)
				<< endmsg;

			set_meta_modifier (alternate_meta_mod);
		}
	}
}

void
Keyboard::get_modifier_masks ()
{
	XModifierKeymap *modifiers;
	KeyCode *keycode;
	int i;
	int bound;

	XDisplayKeycodes (GDK_DISPLAY(), &min_keycode, &max_keycode);

	/* This function builds a lookup table to provide rapid answers to
	   the question: what, if any, modmask, is associated with a given
	   keycode ?
	*/
	
	modifiers = XGetModifierMapping (GDK_DISPLAY());
	
	modifier_masks = new int32_t [max_keycode+1];
	
	keycode = modifiers->modifiermap;

	for (i = 0; i < modifiers->max_keypermod; ++i) { /* shift */
		if (*keycode) {
			modifier_masks[*keycode] = GDK_SHIFT_MASK;
			// cerr << "Shift = " << XKeysymToString (XKeycodeToKeysym  (GDK_DISPLAY(), *keycode, 0)) << endl;
		}
		keycode++;
	}
    
	for (i = 0; i < modifiers->max_keypermod; ++i) keycode++; /* skip lock */
    
	for (i = 0; i < modifiers->max_keypermod; ++i) { /* control */
		if (*keycode) {
			modifier_masks[*keycode] = GDK_CONTROL_MASK;
			// cerr << "Control = " << XKeysymToString (XKeycodeToKeysym  (GDK_DISPLAY(), *keycode, 0)) << endl;
		}
		keycode++;
	}

	bound = 0;
	for (i = 0; i < modifiers->max_keypermod; ++i) { /* mod 1 */
		if (*keycode) {
			modifier_masks[*keycode] = GDK_MOD1_MASK;
			// cerr << "Mod1 = " << XKeysymToString (XKeycodeToKeysym  (GDK_DISPLAY(), *keycode, 0)) << endl;
			bound++;
		}
		keycode++;
	}
#ifdef WARN_ABOUT_DUPLICATE_MODIFIERS
	if (bound > 1) {
		warning << compose (_("You have %1 keys bound to \"mod1\""), bound) << endmsg;
	}
#endif
	bound = 0;
	for (i = 0; i < modifiers->max_keypermod; ++i) { /* mod2 */
		if (*keycode) {
			modifier_masks[*keycode] = GDK_MOD2_MASK;
			check_meta_numlock (*keycode, GDK_MOD2_MASK, "Mod2");
			//cerr << "Mod2 = " << std::hex << (int) *keycode << std::dec << " = " << XKeysymToString (XKeycodeToKeysym  (GDK_DISPLAY(), *keycode, 0)) << endl;
			bound++;
		}
		keycode++; 
	}
#ifdef WARN_ABOUT_DUPLICATE_MODIFIERS
	if (bound > 1) {
		warning << compose (_("You have %1 keys bound to \"mod2\""), bound) << endmsg;
	}
#endif
	bound = 0;
	for (i = 0; i < modifiers->max_keypermod; ++i) { /* mod3 */
		if (*keycode) {
			modifier_masks[*keycode] = GDK_MOD3_MASK;
			check_meta_numlock (*keycode, GDK_MOD3_MASK, "Mod3");
			// cerr << "Mod3 = " << XKeysymToString (XKeycodeToKeysym  (GDK_DISPLAY(), *keycode, 0)) << endl;
			bound++;
		}
		keycode++; 
	}
#ifdef WARN_ABOUT_DUPLICATE_MODIFIERS
	if (bound > 1) {
		warning << compose (_("You have %1 keys bound to \"mod3\""), bound) << endmsg;
	}
#endif
	bound = 0;
	for (i = 0; i < modifiers->max_keypermod; ++i) { /* mod 4 */
		if (*keycode) {
			modifier_masks[*keycode] = GDK_MOD4_MASK;
			check_meta_numlock (*keycode, GDK_MOD4_MASK, "Mod4");
			// cerr << "Mod4 = " << XKeysymToString (XKeycodeToKeysym  (GDK_DISPLAY(), *keycode, 0)) << endl;
			bound++;
		}
		keycode++;
	}
#ifdef WARN_ABOUT_DUPLICATE_MODIFIERS
	if (bound > 1) {
		warning << compose (_("You have %1 keys bound to \"mod4\""), bound) << endmsg;
	}
#endif
	bound = 0;
	for (i = 0; i < modifiers->max_keypermod; ++i) { /* mod 5 */
		if (*keycode) {
			modifier_masks[*keycode] = GDK_MOD5_MASK;
			check_meta_numlock (*keycode, GDK_MOD5_MASK, "Mod5");
			// cerr << "Mod5 = " << XKeysymToString (XKeycodeToKeysym  (GDK_DISPLAY(), *keycode, 0)) << endl;
			bound++;
		}
		keycode++;
	}
#ifdef WARN_ABOUT_DUPLICATE_MODIFIERS
	if (bound > 1) {
		warning << compose (_("You have %1 keys bound to \"mod5\""), bound) << endmsg;
	}
#endif

	XFreeModifiermap (modifiers);
}

gint
Keyboard::enter_window (GdkEventCrossing *ev, KeyboardTarget *kt)
{
	switch (ev->detail) {
	case GDK_NOTIFY_INFERIOR:
		if (debug_keyboard) {
			cerr << "INFERIOR crossing to " << kt->name() << endl;
		}
		break;

	case GDK_NOTIFY_VIRTUAL:
		if (debug_keyboard) {
			cerr << "VIRTUAL crossing to " << kt->name() << endl;
		}
		/* fallthru */

	default:
		if (debug_keyboard) {
			cerr << "REAL crossing to " << kt->name() << endl;
			cerr << "set current target to " << kt->name() << endl;
		}

		set_target (kt);
		check_modifier_state ();
	}

	return FALSE;
}

gint
Keyboard::leave_window (GdkEventCrossing *ev)
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

		set_target (0);
		state.clear ();
		clear_modifier_state ();
	}
	return FALSE;

}

void
Keyboard::register_target (KeyboardTarget *kt)
{
	/* do not register the default - its not meant to be 
	   an actual window, just a fallback if the current
	   target for keyboard events doesn't handle an event.
	*/

	if (kt->name() == X_("default")) {
		return;
	}

	kt->window().enter_notify_event.connect (bind (mem_fun(*this, &Keyboard::enter_window), kt));
	kt->window().leave_notify_event.connect (mem_fun(*this, &Keyboard::leave_window));

	kt->GoingAway.connect (bind (mem_fun(*this, &Keyboard::maybe_unset_target), kt));
	kt->Hiding.connect (bind (mem_fun(*this, &Keyboard::maybe_unset_target), kt));
}

void
Keyboard::set_current_dialog (ArdourDialog* dialog)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Keyboard::set_current_dialog), dialog));
	
	current_dialog = dialog;

	if (current_dialog) {

		if (find (known_dialogs.begin(), known_dialogs.end(), dialog) == known_dialogs.end()) {
			
			current_dialog->GoingAway.connect 
				(bind (mem_fun(*this, &Keyboard::set_current_dialog), 
				       reinterpret_cast<ArdourDialog *>(0)));
			current_dialog->Hiding.connect 
				(bind (mem_fun(*this, &Keyboard::set_current_dialog), 
				       reinterpret_cast<ArdourDialog *>(0)));
			
			current_dialog->unmap_event.connect (mem_fun(*this, &Keyboard::current_dialog_vanished));
			
			known_dialogs.push_back (dialog);
		}
	}
}

gint
Keyboard::current_dialog_vanished (GdkEventAny *ev)
{
	current_dialog = 0;
	state.clear ();
	focus_allowed = false;
	clear_modifier_state ();
	current_prefix = "";

	return FALSE;
}

void
Keyboard::close_current_dialog ()
{
	if (current_dialog) {
		current_dialog->hide ();
	}
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
Keyboard::set_meta_modifier (guint mod)
{
	/* we don't include Meta in the RelevantModifierKeyMask because its not used
	   in the same way as snap_mod, delete_mod etc. the only reason we allow it to be
	   set at all is that X Window has no convention for the keyboard modifier
	   that Meta should use. Some Linux distributions bind NumLock to Mod2, which
	   is our default Meta modifier, and this causes severe problems.
	*/
	Meta = mod;
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

gint
Keyboard::focus_in_handler (GdkEventFocus* ev)
{
	allow_focus (true);
	return FALSE;
}

gint
Keyboard::focus_out_handler (GdkEventFocus* ev)
{
	allow_focus (false);
	return FALSE;
}
