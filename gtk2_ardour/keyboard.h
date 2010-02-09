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

#ifndef __ardour_keyboard_h__
#define __ardour_keyboard_h__

#include "ardour/types.h"
#include "gtkmm2ext/keyboard.h"

#include "selection.h"

class ARDOUR_UI;

class ArdourKeyboard : public Gtkmm2ext::Keyboard
{
  public:
	ArdourKeyboard(ARDOUR_UI& ardour_ui) : ui(ardour_ui) {}

	void setup_keybindings ();

	static Selection::Operation selection_type (guint state);

	ARDOUR_UI& ui;
};

#endif /* __ardour_keyboard_h__ */
