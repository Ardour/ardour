/*
 * Copyright (C) 2005-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2008-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_keyboard_h__
#define __ardour_keyboard_h__

#include <map>
#include <string>

#include "ardour/types.h"
#include "gtkmm2ext/keyboard.h"

#include "selection.h"

class ARDOUR_UI;

class ArdourKeyboard : public Gtkmm2ext::Keyboard
{
public:
	ArdourKeyboard(ARDOUR_UI&);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void setup_keybindings ();

	static Selection::Operation selection_type (guint state);

	ARDOUR_UI& ui;

	void reset_relevant_modifier_key_mask ();

	/** @param state The button state from a GdkEvent.
	 *  @return true if the modifier state indicates snap modifier
	 */
	static bool indicates_snap (guint state);

	/** @param state The button state from a GdkEvent.
	 *  @return true if the modifier state indicates snap delta
	 */
	static bool indicates_snap_delta (guint state);

	/** @param state The button state from a GdkEvent.
	 *  @return true if the modifier state indicates copy modifier
	 */
	static bool indicates_copy (guint state);

	/** @param state The button state from a GdkEvent.
	 *  @return true if the modifier state indicates constraint modifier
	 */
	static bool indicates_constraint (guint state);

	static void set_constraint_modifier (guint);
	/** @return Modifier mask to constrain drags in a particular direction; */
	static ModifierMask constraint_modifier () { return ModifierMask (constraint_mod); }

	static void set_slip_contents_modifier (guint);
	/** @return Modifier mask to move contents rather than region bounds during trim; */
	static ModifierMask slip_contents_modifier () { return ModifierMask (slip_contents_mod); }

	static void set_trim_overlap_modifier (guint);
	/** @return Modifier mask to remove region overlaps during trim; */
	static ModifierMask trim_overlap_modifier () { return ModifierMask (trim_overlap_mod); }

	static void set_trim_anchored_modifier (guint);
	/** @return Modifier mask to use anchored trim; */
	static ModifierMask trim_anchored_modifier () { return ModifierMask (trim_anchored_mod); }

	static void set_fine_adjust_modifier (guint);
	/** @return Modifier mask to fine adjust (control points only atm); */
	static ModifierMask fine_adjust_modifier () { return ModifierMask (fine_adjust_mod); }

	static void set_push_points_modifier (guint);
	/** @return Modifier mask to push proceeding points; */
	static ModifierMask push_points_modifier () { return ModifierMask (push_points_mod); }

	static void set_note_size_relative_modifier (guint);
	/** @return Modifier mask to resize notes relatively; */
	static ModifierMask note_size_relative_modifier () { return ModifierMask (note_size_relative_mod); }
private:
	static guint     constraint_mod;
	static guint     slip_contents_mod;
	static guint     trim_overlap_mod;
	static guint     trim_anchored_mod;
	static guint     fine_adjust_mod;
	static guint     push_points_mod;
	static guint     note_size_relative_mod;

	void find_bindings_files (std::map<std::string,std::string>& files);
};

#endif /* __ardour_keyboard_h__ */
