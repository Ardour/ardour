/*
  Copyright (C) 2014 Paul Davis
  Author: David Robillard

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

#ifndef __gtk2_ardour_note_select_dialog_h__
#define __gtk2_ardour_note_select_dialog_h__

#include <stdint.h>
#include "ardour_dialog.h"
#include "gtk_pianokeyboard.h"

class NoteSelectDialog : public ArdourDialog
{
public:
	NoteSelectDialog();

	uint8_t note_number() const { return _note_number; }

	void note_on_event_handler(int note);

private:
	PianoKeyboard* _piano;
	Gtk::Widget*   _pianomm;
	uint8_t        _note_number;
};

#endif /* __gtk2_ardour_note_select_dialog_h__ */
