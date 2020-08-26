/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
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

#include <cassert>
#include <iostream>

#include "piano_key_bindings.h"

PianoKeyBindings::PianoKeyBindings ()
{
	bind_keys_qwerty ();
}

PianoKeyBindings::~PianoKeyBindings ()
{
}

void
PianoKeyBindings::set_layout (Layout layout)
{
	switch (layout) {
		case NO_KEYS:
			clear_notes ();
			break;
		case QWERTY:
			bind_keys_qwerty ();
			break;
		case QWERTZ:
			bind_keys_qwertz ();
			break;
		case AZERTY:
			bind_keys_azerty ();
			break;
		case DVORAK:
			bind_keys_dvorak ();
			break;
		case S_QWERTY:
			bind_keys_basic_qwerty ();
			break;
		case S_QWERTZ:
			bind_keys_basic_qwertz ();
			break;
	}
}

int
PianoKeyBindings::key_binding (const char* key) const
{
	std::map<std::string, int>::const_iterator kv;
	if (key && (kv = _key_bindings.find (key)) != _key_bindings.end ()) {
		return kv->second;
	}
	return -1;
}

const char*
PianoKeyBindings::note_binding (int note) const
{
	std::map<int, std::string>::const_iterator kv = _note_bindings.find (note);
	if (kv == _note_bindings.end ()) {
		return 0;
	}
	return kv->second.c_str();
}

PianoKeyBindings::Layout
PianoKeyBindings::layout (std::string const& l)
{
	if (l == "QWERTY") {
		return QWERTY;
	} else if (l == "QWERTZ") {
		return QWERTZ;
	} else if (l == "AZERTY") {
		return AZERTY;
	} else if (l == "DVORAK") {
		return DVORAK;
	} else if (l == "QWERTY Single") {
		return S_QWERTY;
	} else if (l == "QWERTZ Single") {
		return S_QWERTZ;
	} else if (l == "None") {
		return NO_KEYS;
	}

	// Unrecognized keyboard layout, maybe an assert is too stringent though
	assert(false);
	return NO_KEYS;
}

const char*
PianoKeyBindings::get_keycode (GdkEventKey* event)
{
	/* We're not using event->keyval, because we need keyval with level set to 0.
	   E.g. if user holds Shift and presses '7', we want to get a '7', not '&'. */

#ifdef __APPLE__
	/* gdkkeys-quartz.c does not implement gdk_keymap_lookup_key */
	guint keyval;
	gdk_keymap_translate_keyboard_state (NULL, event->hardware_keycode,
	                                     (GdkModifierType)0, 0,
	                                     &keyval, NULL, NULL, NULL);
#else
	GdkKeymapKey kk;
	kk.keycode = event->hardware_keycode;
	kk.level   = 0;
	kk.group   = 0;

	guint keyval = gdk_keymap_lookup_key (NULL, &kk);
#endif
	return gdk_keyval_name (gdk_keyval_to_lower (keyval));
}

void
PianoKeyBindings::bind_key (const char* key, int note)
{
	_key_bindings[key]   = note;
	_note_bindings[note] = key;
}

void
PianoKeyBindings::clear_notes ()
{
	_key_bindings.clear ();
	_note_bindings.clear ();
}

void
PianoKeyBindings::bind_keys_qwerty ()
{
	clear_notes ();

	bind_key ("space", 128);
	bind_key ("Tab", 129);

	/* Lower keyboard row - "zxcvbnm". */
	bind_key ("z", 12); /* C0 */
	bind_key ("s", 13);
	bind_key ("x", 14);
	bind_key ("d", 15);
	bind_key ("c", 16);
	bind_key ("v", 17);
	bind_key ("g", 18);
	bind_key ("b", 19);
	bind_key ("h", 20);
	bind_key ("n", 21);
	bind_key ("j", 22);
	bind_key ("m", 23);

	/* Upper keyboard row, first octave - "qwertyu". */
	bind_key ("q", 24);
	bind_key ("2", 25);
	bind_key ("w", 26);
	bind_key ("3", 27);
	bind_key ("e", 28);
	bind_key ("r", 29);
	bind_key ("5", 30);
	bind_key ("t", 31);
	bind_key ("6", 32);
	bind_key ("y", 33);
	bind_key ("7", 34);
	bind_key ("u", 35);

	/* Upper keyboard row, the rest - "iop". */
	bind_key ("i", 36);
	bind_key ("9", 37);
	bind_key ("o", 38);
	bind_key ("0", 39);
	bind_key ("p", 40);

	/* ignore */
	bind_key ("a", -2);
	bind_key ("f", -3);
	bind_key ("1", -4);
	bind_key ("4", -5);
	bind_key ("8", -6);
}

void
PianoKeyBindings::bind_keys_qwertz ()
{
	bind_keys_qwerty ();

	/* The only difference between QWERTY and QWERTZ is that the "y" and "z" are swapped together. */
	bind_key ("y", 12);
	bind_key ("z", 33);
}

void
PianoKeyBindings::bind_keys_azerty ()
{
	clear_notes ();

	bind_key ("space", 128);
	bind_key ("Tab", 129);

	/* Lower keyboard row - "wxcvbn,". */
	bind_key ("w", 12); /* C0 */
	bind_key ("s", 13);
	bind_key ("x", 14);
	bind_key ("d", 15);
	bind_key ("c", 16);
	bind_key ("v", 17);
	bind_key ("g", 18);
	bind_key ("b", 19);
	bind_key ("h", 20);
	bind_key ("n", 21);
	bind_key ("j", 22);
	bind_key ("comma", 23);

	/* Upper keyboard row, first octave - "azertyu". */
	bind_key ("a", 24);
	bind_key ("eacute", 25);
	bind_key ("z", 26);
	bind_key ("quotedbl", 27);
	bind_key ("e", 28);
	bind_key ("r", 29);
	bind_key ("parenleft", 30);
	bind_key ("t", 31);
	bind_key ("minus", 32);
	bind_key ("y", 33);
	bind_key ("egrave", 34);
	bind_key ("u", 35);

	/* Upper keyboard row, the rest - "iop". */
	bind_key ("i", 36);
	bind_key ("ccedilla", 37);
	bind_key ("o", 38);
	bind_key ("agrave", 39);
	bind_key ("p", 40);
}

void
PianoKeyBindings::bind_keys_dvorak ()
{
	clear_notes ();

	bind_key ("space", 128);
	bind_key ("Tab", 129);

	/* Lower keyboard row - ";qjkxbm". */
	bind_key ("semicolon", 12); /* C0 */
	bind_key ("o", 13);
	bind_key ("q", 14);
	bind_key ("e", 15);
	bind_key ("j", 16);
	bind_key ("k", 17);
	bind_key ("i", 18);
	bind_key ("x", 19);
	bind_key ("d", 20);
	bind_key ("b", 21);
	bind_key ("h", 22);
	bind_key ("m", 23);
	bind_key ("w", 24); /* overlaps with upper row */
	bind_key ("n", 25);
	bind_key ("v", 26);
	bind_key ("s", 27);
	bind_key ("z", 28);

	/* Upper keyboard row, first octave - "',.pyfg". */
	bind_key ("apostrophe", 24);
	bind_key ("2", 25);
	bind_key ("comma", 26);
	bind_key ("3", 27);
	bind_key ("period", 28);
	bind_key ("p", 29);
	bind_key ("5", 30);
	bind_key ("y", 31);
	bind_key ("6", 32);
	bind_key ("f", 33);
	bind_key ("7", 34);
	bind_key ("g", 35);

	/* Upper keyboard row, the rest - "crl". */
	bind_key ("c", 36);
	bind_key ("9", 37);
	bind_key ("r", 38);
	bind_key ("0", 39);
	bind_key ("l", 40);
#if 0
	bind_key ("slash", 41); /* extra F */
	bind_key ("bracketright", 42);
	bind_key ("equal", 43);
#endif
}

void
PianoKeyBindings::bind_keys_basic_qwerty ()
{
	clear_notes ();

	bind_key ("space", 128);
	bind_key ("Tab", 129);

	/* simple - middle rows only */
	bind_key ("a", 12); /* C0 */
	bind_key ("w", 13);
	bind_key ("s", 14);
	bind_key ("e", 15);
	bind_key ("d", 16);
	bind_key ("f", 17);
	bind_key ("t", 18);
	bind_key ("g", 19);
	bind_key ("y", 20);
	bind_key ("h", 21);
	bind_key ("u", 22);
	bind_key ("j", 23);

	bind_key ("k", 24); /* C1 */
	bind_key ("o", 25);
	bind_key ("l", 26);
	bind_key ("p", 27);
	bind_key ("semicolon", 28);
	bind_key ("apostrophe", 29);
}

void
PianoKeyBindings::bind_keys_basic_qwertz ()
{
	clear_notes ();

	bind_key ("space", 128);
	bind_key ("Tab", 129);

	/* simple - middle rows only */
	bind_key ("a", 12); /* C0 */
	bind_key ("w", 13);
	bind_key ("s", 14);
	bind_key ("e", 15);
	bind_key ("d", 16);
	bind_key ("f", 17);
	bind_key ("t", 18);
	bind_key ("g", 19);
	bind_key ("z", 20);
	bind_key ("h", 21);
	bind_key ("u", 22);
	bind_key ("j", 23);

	bind_key ("k", 24); /* C1 */
	bind_key ("o", 25);
	bind_key ("l", 26);
	bind_key ("p", 27);
	bind_key ("semicolon", 28);
	bind_key ("apostrophe", 29);
}
