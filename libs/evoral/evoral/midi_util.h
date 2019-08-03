/*
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#ifndef EVORAL_MIDI_UTIL_H
#define EVORAL_MIDI_UTIL_H

#include <iostream>

#include <stdint.h>
#include <string>
#include <sys/types.h>
#include <assert.h>

#include "evoral/visibility.h"
#include "evoral/midi_events.h"

namespace Evoral {


/** Return the size of the given event including the status byte,
 * or -1 if unknown (e.g. sysex)
 */
static inline int
midi_event_size(uint8_t status)
{
	// if we have a channel event
	if (status >= 0x80 && status < 0xF0) {
		status &= 0xF0; // mask off the channel
	}

	switch (status) {
	case MIDI_CMD_NOTE_OFF:
	case MIDI_CMD_NOTE_ON:
	case MIDI_CMD_NOTE_PRESSURE:
	case MIDI_CMD_CONTROL:
	case MIDI_CMD_BENDER:
	case MIDI_CMD_COMMON_SONG_POS:
		return 3;

	case MIDI_CMD_PGM_CHANGE:
	case MIDI_CMD_CHANNEL_PRESSURE:
	case MIDI_CMD_COMMON_MTC_QUARTER:
	case MIDI_CMD_COMMON_SONG_SELECT:
		return 2;

	case MIDI_CMD_COMMON_TUNE_REQUEST:
	case MIDI_CMD_COMMON_SYSEX_END:
	case MIDI_CMD_COMMON_CLOCK:
	case MIDI_CMD_COMMON_START:
	case MIDI_CMD_COMMON_CONTINUE:
	case MIDI_CMD_COMMON_STOP:
	case MIDI_CMD_COMMON_SENSING:
	case MIDI_CMD_COMMON_RESET:
		return 1;

	case MIDI_CMD_COMMON_SYSEX:
		std::cerr << "event size called for sysex\n";
		return -1;
	}

	std::cerr << "event size called for unknown status byte " << std::hex << (int) status << "\n";
	return -1;
}

/** Return the size of the given event including the status byte,
 * or -1 if event is illegal.
 */
static inline int
midi_event_size(const uint8_t* buffer)
{
	uint8_t status = buffer[0];

	// Mask off channel if applicable
	if (status >= 0x80 && status < 0xF0) {
		status &= 0xF0;
	}

	// see http://www.midi.org/techspecs/midimessages.php
	if (status == MIDI_CMD_COMMON_SYSEX) {
		int end;

		for (end = 1; buffer[end] != MIDI_CMD_COMMON_SYSEX_END; end++) {
			if ((buffer[end] & 0x80) != 0) {
				return -1;
			}
		}
		assert(buffer[end] == MIDI_CMD_COMMON_SYSEX_END);
		return end + 1;
	} else {
		return midi_event_size(status);
	}
}

/** Return true iff the given buffer is a valid MIDI event.
 * \a len must be exactly correct for the contents of \a buffer
 */
static inline bool
midi_event_is_valid(const uint8_t* buffer, size_t len)
{
	uint8_t status = buffer[0];
	if (status < 0x80) {
		return false;
	}
	const int size = midi_event_size(buffer);
	if (size < 0 || (size_t)size != len) {
		return false;
	}
	if (status < 0xf0) {
		/* Channel messages: all start with status byte followed by
		 * non status bytes.
		 */
		for (size_t i = 1; i < len; ++i) {
			if ((buffer[i] & 0x80) != 0) {
				return false;  // Non-status byte has MSb set
			}
		}
	}
	return true;
}

} // namespace Evoral

#endif // EVORAL_MIDI_UTIL_H

