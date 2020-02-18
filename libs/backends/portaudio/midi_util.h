/*
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef MIDI_UTIL_H
#define MIDI_UTIL_H

#include <stdint.h>

struct MidiEventHeader {
	uint64_t time;
	size_t size;
	MidiEventHeader (const uint64_t t, const size_t s)
	    : time (t)
	    , size (s)
	{
	}
};

// rename to get_midi_message_size?
// @return -1 to indicate error
int get_midi_msg_length (uint8_t status_byte);

#endif
