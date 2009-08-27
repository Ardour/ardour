/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright (C) 2009 Paul Davis
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "evoral/midi_util.h"
#include <cstdio>

namespace Evoral {

std::string
midi_note_name (uint8_t val)
{
	if (val > 127) {
		return "???";
	}

	static const char* notes[] = {
		"c",
		"c#",
		"d",
		"d#",
		"e",
		"f",
		"f#",
		"g",
		"a",
		"a#",
		"b",
		"b#"
	};

	int octave = val/12;
	static char buf[8];
	
	val -= octave*12;

	snprintf (buf, sizeof (buf), "%s%d", notes[val], octave);
	return buf;
}

}
