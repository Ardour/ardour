/*
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk2_ardour_note_select_dialog_h__
#define __gtk2_ardour_note_select_dialog_h__

#include <stdint.h>
#include "ardour_dialog.h"
#include "pianokeyboard.h"

class NoteSelectDialog : public ArdourDialog
{
public:
	NoteSelectDialog();

	uint8_t note_number() const { return _note_number; }

private:
	APianoKeyboard _piano;
	uint8_t        _note_number;

	void note_on_event_handler(int, int);
};

#endif /* __gtk2_ardour_note_select_dialog_h__ */
