/* Names of standard MIDI events and controllers.
 * Copyright (C) 2007-2008 David Robillard <http://drobilla.net>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef __midinames_h__
#define __midinames_h__

#include <stdint.h>

#include "events.h"

/** \group midi
 */

/** Pass this a symbol defined in events.h (e.g. MIDI_CTL_PAN) to get the
 * short name of a MIDI event/controller according to General MIDI.
 */
inline static const char* midi_name(uint8_t status)
{
	switch (status) {
	
	case MIDI_CMD_NOTE_OFF:
		return "Note Off"; break;
	case MIDI_CMD_NOTE_ON:
		return "Note On"; break;
	case MIDI_CMD_NOTE_PRESSURE:
		return "Key Pressure"; break;
	case MIDI_CMD_CONTROL:
		return "Control Change"; break;
	case MIDI_CMD_PGM_CHANGE:
		return "Program Change"; break;
	case MIDI_CMD_CHANNEL_PRESSURE:
		return "Channel Pressure"; break;
	case MIDI_CMD_BENDER:
		return "Pitch Bender"; break;

	case MIDI_CMD_COMMON_SYSEX:
		return "Sysex (System Exclusive) Begin"; break;
	case MIDI_CMD_COMMON_MTC_QUARTER:
		return "MTC Quarter Frame"; break;
	case MIDI_CMD_COMMON_SONG_POS:
		return "Song Position"; break;
	case MIDI_CMD_COMMON_SONG_SELECT:
		return "Song Select"; break;
	case MIDI_CMD_COMMON_TUNE_REQUEST:
		return "Tune Request"; break;
	case MIDI_CMD_COMMON_SYSEX_END:
		return "End of Sysex"; break;
	case MIDI_CMD_COMMON_CLOCK:
		return "Clock"; break;
	case MIDI_CMD_COMMON_TICK:
		return "Tick"; break;
	case MIDI_CMD_COMMON_START:
		return "Start"; break;
	case MIDI_CMD_COMMON_CONTINUE:
		return "Continue"; break;
	case MIDI_CMD_COMMON_STOP:
		return "Stop"; break;
	case MIDI_CMD_COMMON_SENSING:
		return "Active Sensing"; break;
	case MIDI_CMD_COMMON_RESET:
		return "Reset"; break;

	case MIDI_CTL_MSB_BANK:
		return "Bank Select (Coarse)"; break;
	case MIDI_CTL_MSB_MODWHEEL:
		return "Modulation (Coarse)"; break;
	case MIDI_CTL_MSB_BREATH:
		return "Breath (Coarse)"; break;
	case MIDI_CTL_MSB_FOOT:
		return "Foot (Coarse)"; break;
	case MIDI_CTL_MSB_PORTAMENTO_TIME:
		return "Portamento Time (Coarse)"; break;
	case MIDI_CTL_MSB_DATA_ENTRY:
		return "Data Entry (Coarse)"; break;
	case MIDI_CTL_MSB_MAIN_VOLUME:
		return "Main Volume (Coarse)"; break;
	case MIDI_CTL_MSB_BALANCE:
		return "Balance (Coarse)"; break;
	case MIDI_CTL_MSB_PAN:
		return "Pan (Coarse)"; break;
	case MIDI_CTL_MSB_EXPRESSION:
		return "Expression (Coarse)"; break;
	case MIDI_CTL_MSB_EFFECT1:
		return "Effect 1 (Coarse)"; break;
	case MIDI_CTL_MSB_EFFECT2:
		return "Effect 2 (Coarse)"; break;
	case MIDI_CTL_MSB_GENERAL_PURPOSE1:
		return "General Purpose 1 (Coarse)"; break;
	case MIDI_CTL_MSB_GENERAL_PURPOSE2:
		return "General Purpose 2 (Coarse)"; break;
	case MIDI_CTL_MSB_GENERAL_PURPOSE3:
		return "General Purpose 3 (Coarse)"; break;
	case MIDI_CTL_MSB_GENERAL_PURPOSE4:
		return "General Purpose 4 (Coarse)"; break;
	case MIDI_CTL_LSB_BANK:
		return "Bank Select (Fine)"; break;
	case MIDI_CTL_LSB_MODWHEEL:
		return "Modulation (Fine)"; break;
	case MIDI_CTL_LSB_BREATH:
		return "Breath (Fine)"; break;
	case MIDI_CTL_LSB_FOOT:
		return "Foot (Fine)"; break;
	case MIDI_CTL_LSB_PORTAMENTO_TIME:
		return "Portamento Time (Fine)"; break;
	case MIDI_CTL_LSB_DATA_ENTRY:
		return "Data Entry (Fine)"; break;
	case MIDI_CTL_LSB_MAIN_VOLUME:
		return "Main Volume (Fine)"; break;
	case MIDI_CTL_LSB_BALANCE:
		return "Balance (Fine)"; break;
	case MIDI_CTL_LSB_PAN:
		return "Pan (Fine)"; break;
	case MIDI_CTL_LSB_EXPRESSION:
		return "Expression (Fine)"; break;
	case MIDI_CTL_LSB_EFFECT1:
		return "Effect 1 (Fine)"; break;
	case MIDI_CTL_LSB_EFFECT2:
		return "Effect 2 (Fine)"; break;
	case MIDI_CTL_LSB_GENERAL_PURPOSE1:
		return "General Purpose 1 (Fine)"; break;
	case MIDI_CTL_LSB_GENERAL_PURPOSE2:
		return "General Purpose 2 (Fine)"; break;
	case MIDI_CTL_LSB_GENERAL_PURPOSE3:
		return "General Purpose 3 (Fine)"; break;
	case MIDI_CTL_LSB_GENERAL_PURPOSE4:
		return "General Purpose 4 (Fine)"; break;
	case MIDI_CTL_SUSTAIN:
		return "Sustain Pedal"; break;
	case MIDI_CTL_PORTAMENTO:
		return "Portamento"; break;
	case MIDI_CTL_SOSTENUTO:
		return "Sostenuto"; break;
	case MIDI_CTL_SOFT_PEDAL:
		return "Soft Pedal"; break;
	case MIDI_CTL_LEGATO_FOOTSWITCH:
		return "Legato Foot Switch"; break;
	case MIDI_CTL_HOLD2:
		return "Hold2"; break;
	case MIDI_CTL_SC1_SOUND_VARIATION:
		return "Sound Variation"; break;
	case MIDI_CTL_SC2_TIMBRE:
		return "Sound Timbre"; break;
	case MIDI_CTL_SC3_RELEASE_TIME:
		return "Sound Release Time"; break;
	case MIDI_CTL_SC4_ATTACK_TIME:
		return "Sound Attack Time"; break;
	case MIDI_CTL_SC5_BRIGHTNESS:
		return "Sound Brightness"; break;
	case MIDI_CTL_SC6:
		return "Sound Control 6"; break;
	case MIDI_CTL_SC7:
		return "Sound Control 7"; break;
	case MIDI_CTL_SC8:
		return "Sound Control 8"; break;
	case MIDI_CTL_SC9:
		return "Sound Control 9"; break;
	case MIDI_CTL_SC10:
		return "Sound Control 10"; break;
	case MIDI_CTL_GENERAL_PURPOSE5:
		return "General Purpose 5"; break;
	case MIDI_CTL_GENERAL_PURPOSE6:
		return "General Purpose 6"; break;
	case MIDI_CTL_GENERAL_PURPOSE7:
		return "General Purpose 7"; break;
	case MIDI_CTL_GENERAL_PURPOSE8:
		return "General Purpose 8"; break;
	case MIDI_CTL_PORTAMENTO_CONTROL:
		return "Portamento Control"; break;
	case MIDI_CTL_E1_REVERB_DEPTH:
		return "Reverb Depth"; break;
	case MIDI_CTL_E2_TREMOLO_DEPTH:
		return "Tremolo Depth"; break;
	case MIDI_CTL_E3_CHORUS_DEPTH:
		return "Chorus Depth"; break;
	case MIDI_CTL_E4_DETUNE_DEPTH:
		return "Detune Depth"; break;
	case MIDI_CTL_E5_PHASER_DEPTH:
		return "Phaser Depth"; break;
	case MIDI_CTL_DATA_INCREMENT:
		return "Data Increment"; break;
	case MIDI_CTL_DATA_DECREMENT:
		return "Data Decrement"; break;
	case MIDI_CTL_NONREG_PARM_NUM_LSB:
		return "Non-registered Parameter Number"; break;
	case MIDI_CTL_NONREG_PARM_NUM_MSB:
		return "Non-registered Narameter Number"; break;
	case MIDI_CTL_REGIST_PARM_NUM_LSB:
		return "Registered Parameter Number"; break;
	case MIDI_CTL_REGIST_PARM_NUM_MSB:
		return "Registered Parameter Number"; break;
	case MIDI_CTL_ALL_SOUNDS_OFF:
		return "All Sounds Off"; break;
	case MIDI_CTL_RESET_CONTROLLERS:
		return "Reset Controllers"; break;
	case MIDI_CTL_LOCAL_CONTROL_SWITCH:
		return "Local Keyboard on/off"; break;
	case MIDI_CTL_ALL_NOTES_OFF:
		return "All Notes Off"; break;
	case MIDI_CTL_OMNI_OFF:
		return "Omni Off"; break;
	case MIDI_CTL_OMNI_ON:
		return "Omni On"; break;
	case MIDI_CTL_MONO:
		return "Monophonic Mode"; break;
	case MIDI_CTL_POLY:
		return "Polyphonic Mode"; break;
	default:
		break;
	}

	return "Unnamed";
	
}

#endif /* __midinames_h__ */

