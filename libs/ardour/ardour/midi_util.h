/*
    Copyright (C) 2006 Paul Davis
	Written by Dave Robillard, 2006

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

#ifndef __ardour_midi_util_h__ 
#define __ardour_midi_util_h__

#include <ardour/midi_events.h>

namespace ARDOUR {

/** Return the size of the given event NOT including the status byte,
 * or -1 if unknown (eg sysex)
 */
int
midi_event_size(unsigned char status)
{
	if (status >= 0x80 && status <= 0xE0) {
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

} // namespace ARDOUR

#endif /* __ardour_midi_util_h__ */
