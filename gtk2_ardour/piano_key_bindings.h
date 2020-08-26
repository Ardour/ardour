/*
 * Copyright (C) 2010-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef _PIANO_KEY_BINDINGS_H_
#define _PIANO_KEY_BINDINGS_H_

#include <map>
#include <string>

#include <gtk/gtk.h>

/*
 * Class for mapping PC keyboard keys to note pitches. Used by the
 * Virtual MIDI Keyboard.
 */
class PianoKeyBindings
{
public:
	PianoKeyBindings ();
	~PianoKeyBindings ();

	enum Layout {
		NO_KEYS,
		QWERTY,
		QWERTZ,
		AZERTY,
		DVORAK,
		S_QWERTY,
		S_QWERTZ
	};

	void set_layout (Layout layout);
	int  key_binding (const char* key) const;
	const char* note_binding (int note) const;

	static Layout layout (std::string const& l);
	static const char* get_keycode (GdkEventKey* event);

private:
	void bind_key (const char* key, int note);
	void clear_notes ();

	void bind_keys_qwerty ();
	void bind_keys_qwertz ();
	void bind_keys_azerty ();
	void bind_keys_dvorak ();

	void bind_keys_basic_qwerty ();
	void bind_keys_basic_qwertz ();

	std::map<std::string, int> _key_bindings;  /**< Table used to translate from PC keyboard character to MIDI note number. */
	std::map<int, std::string> _note_bindings; /**< Table to translate from MIDI note number to PC keyboard character. */
};

#endif
