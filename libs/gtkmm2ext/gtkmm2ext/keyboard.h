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

#ifndef __libgtkmm2ext_keyboard_h__
#define __libgtkmm2ext_keyboard_h__

#include <map>
#include <vector>
#include <string>

#include <sigc++/signal.h>
#include <gtk/gtk.h>
#include <gtkmm/accelkey.h>

#include "pbd/stateful.h"

#include "gtkmm2ext/visibility.h"

namespace Gtk {
	class Window;
}

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API Keyboard : public sigc::trackable, PBD::Stateful
{
  public:
	Keyboard ();
	~Keyboard ();

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	virtual void setup_keybindings () = 0;

	typedef std::vector<uint32_t> State;
	typedef uint32_t ModifierMask;

	static uint32_t PrimaryModifier;
	static uint32_t SecondaryModifier;
	static uint32_t TertiaryModifier;
	static uint32_t Level4Modifier;
	static uint32_t CopyModifier;
	static uint32_t RangeSelectModifier;
	static uint32_t GainFineScaleModifier;
	static uint32_t GainExtraFineScaleModifier;

	// Modifiers for scroll wheel
	static uint32_t ScrollZoomVerticalModifier;
	static uint32_t ScrollZoomHorizontalModifier;
	static uint32_t ScrollHorizontalModifier;

	static const char* primary_modifier_name ();
	static const char* secondary_modifier_name ();
	static const char* tertiary_modifier_name ();
	static const char* level4_modifier_name ();
	static const char* copy_modifier_name ();
	static const char* rangeselect_modifier_name ();

	static void set_primary_modifier (uint32_t newval) {
		set_modifier (newval, PrimaryModifier);
	}
	static void set_secondary_modifier (uint32_t newval) {
		set_modifier (newval, SecondaryModifier);
	}
	static void set_tertiary_modifier (uint32_t newval) {
		set_modifier (newval, TertiaryModifier);
	}
	static void set_level4_modifier (uint32_t newval) {
		set_modifier (newval, Level4Modifier);
	}
	static void set_copy_modifier (uint32_t newval) {
		set_modifier (newval, CopyModifier);
	}
	static void set_range_select_modifier (uint32_t newval) {
		set_modifier (newval, RangeSelectModifier);
	}

	bool key_is_down (uint32_t keyval);

	static GdkModifierType RelevantModifierKeyMask;

	static bool no_modifier_keys_pressed(GdkEventButton* ev) {
		return (ev->state & RelevantModifierKeyMask) == 0;
	}

	static bool no_modifier_keys_pressed(GdkEventKey* ev) {
		return (ev->state & RelevantModifierKeyMask) == 0;
	}

	bool leave_window (GdkEventCrossing *ev, Gtk::Window*);
	bool enter_window (GdkEventCrossing *ev, Gtk::Window*);
	bool focus_in_window (GdkEventFocus *ev, Gtk::Window*);
	bool focus_out_window (GdkEventFocus *ev, Gtk::Window*);

	static bool modifier_state_contains (guint state, ModifierMask);
	static bool modifier_state_equals   (guint state, ModifierMask);

	static bool no_modifiers_active (guint state);

	static void set_snap_modifier (guint);
	/** @return Modifier mask to temporarily toggle grid setting; with this modifier
	 *  - magnetic or normal grid should become no grid and
	 *  - no grid should become normal grid
	 */
	static ModifierMask snap_modifier () { return ModifierMask (snap_mod); }

	static void set_snap_delta_modifier (guint);
	/** @return Modifier mask to temporarily toggle between relative and absolute grid setting;
	 */
	static ModifierMask snap_delta_modifier () { return ModifierMask (snap_delta_mod); }

	static void set_trim_contents_modifier (guint);
	/** @return Modifier mask to move contents rather than region bounds during trim;
	 */
	static ModifierMask trim_contents_modifier () { return ModifierMask (trim_contents_mod); }

	static void set_trim_overlap_modifier (guint);
	/** @return Modifier mask to remove region overlaps during trim;
	 */
	static ModifierMask trim_overlap_modifier () { return ModifierMask (trim_overlap_mod); }

	static void set_trim_anchored_modifier (guint);
	/** @return Modifier mask to use anchored trim;
	 */
	static ModifierMask trim_anchored_modifier () { return ModifierMask (trim_anchored_mod); }

	static void set_fine_adjust_modifier (guint);
	/** @return Modifier mask to fine adjust (control points only atm);
	 */
	static ModifierMask fine_adjust_modifier () { return ModifierMask (fine_adjust_mod); }

	static void set_push_points_modifier (guint);
	/** @return Modifier mask to push proceeding points;
	 */
	static ModifierMask push_points_modifier () { return ModifierMask (push_points_mod); }

	static void set_note_size_relative_modifier (guint);
	/** @return Modifier mask to resize notes relatively;
	 */
	static ModifierMask note_size_relative_modifier () { return ModifierMask (note_size_relative_mod); }

	static void set_trim_jump_modifier (guint);
	/** @return Modifier mask to jump position after trim;
	 */
	static ModifierMask trim_jump_modifier () { return ModifierMask (trim_jump_mod); }

	static guint edit_button() { return edit_but; }
	static void set_edit_button (guint);
	static guint edit_modifier() { return edit_mod; }
	static void set_edit_modifier(guint);

	static guint delete_button() { return delete_but; }
	static void set_delete_button(guint);
	static guint delete_modifier() { return delete_mod; }
	static void set_delete_modifier(guint);

	static guint insert_note_button() { return insert_note_but; }
	static void set_insert_note_button (guint);
	static guint insert_note_modifier() { return insert_note_mod; }
	static void set_insert_note_modifier(guint);
	
	static bool is_edit_event (GdkEventButton*);
	static bool is_delete_event (GdkEventButton*);
	static bool is_insert_note_event (GdkEventButton*);
	static bool is_context_menu_event (GdkEventButton*);
	static bool is_button2_event (GdkEventButton*);

	static Keyboard& the_keyboard() { return *_the_keyboard; }

	static bool some_magic_widget_has_focus ();
	static void magic_widget_grab_focus ();
	static void magic_widget_drop_focus ();
	static Gtk::Window* get_current_window () { return current_window; };

	static void close_current_dialog ();

	static void keybindings_changed ();
	static void save_keybindings ();
	static bool load_keybindings (std::string path);
	static void set_can_save_keybindings (bool yn);
	static std::string current_binding_name () { return _current_binding_name; }
	static std::map<std::string,std::string> binding_files;

	int reset_bindings ();

	struct AccelKeyLess {
	    bool operator() (const Gtk::AccelKey a, const Gtk::AccelKey b) const {
		    if (a.get_key() != b.get_key()) {
			    return a.get_key() < b.get_key();
		    } else {
			    return a.get_mod() < b.get_mod();
		    }
	    }
	};

	sigc::signal0<void> ZoomVerticalModifierReleased;

  protected:
	static Keyboard* _the_keyboard;

	guint           snooper_id;
	State           state;

	static guint     edit_but;
	static guint     edit_mod;
	static guint     delete_but;
	static guint     delete_mod;
	static guint     insert_note_but;
	static guint     insert_note_mod;
	static guint     snap_mod;
	static guint     snap_delta_mod;
	static guint     trim_contents_mod;
	static guint     trim_overlap_mod;
	static guint     trim_anchored_mod;
	static guint     fine_adjust_mod;
	static guint     push_points_mod;
	static guint     note_size_relative_mod;
	static guint     trim_jump_mod;
	static guint     button2_modifiers;
	static Gtk::Window* current_window;
	static std::string user_keybindings_path;
	static bool can_save_keybindings;
	static bool bindings_changed_after_save_became_legal;
	static std::string _current_binding_name;

	typedef std::pair<std::string,std::string> two_strings;

	static std::map<Gtk::AccelKey,two_strings,AccelKeyLess> release_keys;

	static gint _snooper (GtkWidget*, GdkEventKey*, gpointer);
	gint snooper (GtkWidget*, GdkEventKey*);

	static void set_modifier (uint32_t newval, uint32_t& variable);

	static bool _some_magic_widget_has_focus;
};

} /* namespace */

#endif /* __libgtkmm2ext_keyboard_h__ */
