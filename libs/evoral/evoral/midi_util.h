/* This file is part of Evoral.
 * Copyright(C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright(C) 2000-2008 Paul Davis
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or(at your option) any later
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

#ifndef EVORAL_MIDI_UTIL_H
#define EVORAL_MIDI_UTIL_H

#include <evoral/midi_events.h>

namespace Evoral {

/** Return the size of the given event NOT including the status byte,
 * or -1 if unknown (eg sysex)
 */
static inline int
midi_event_size(unsigned char status)
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
		return 2;

	case MIDI_CMD_PGM_CHANGE:
	case MIDI_CMD_CHANNEL_PRESSURE:
	case MIDI_CMD_COMMON_MTC_QUARTER:
	case MIDI_CMD_COMMON_SONG_SELECT:
		return 1;

	case MIDI_CMD_COMMON_TUNE_REQUEST:
	case MIDI_CMD_COMMON_SYSEX_END:
	case MIDI_CMD_COMMON_CLOCK:
	case MIDI_CMD_COMMON_START:
	case MIDI_CMD_COMMON_CONTINUE:
	case MIDI_CMD_COMMON_STOP:
	case MIDI_CMD_COMMON_SENSING:
	case MIDI_CMD_COMMON_RESET:
		return 0;
	
	case MIDI_CMD_COMMON_SYSEX:
		return -1;
	}

	return -1;
}

} // namespace Evoral

#endif // EVORAL_MIDI_UTIL_H

