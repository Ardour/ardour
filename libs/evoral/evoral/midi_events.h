/* Definitions to ease working with raw MIDI.
 *
 * Adapted from ALSA's asounddef.h
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

#ifndef EVORAL_MIDI_EVENTS_H
#define EVORAL_MIDI_EVENTS_H


/**
 * \defgroup midi MIDI Definitions
 * MIDI command and controller number definitions.
 * \{
 */


// Controllers
#define MIDI_CTL_MSB_BANK               0x00 /**< Bank Selection */
#define MIDI_CTL_MSB_MODWHEEL           0x01 /**< Modulation */
#define MIDI_CTL_MSB_BREATH             0x02 /**< Breath */
#define MIDI_CTL_MSB_FOOT               0x04 /**< Foot */
#define MIDI_CTL_MSB_PORTAMENTO_TIME    0x05 /**< Portamento Time */
#define MIDI_CTL_MSB_DATA_ENTRY         0x06 /**< Data Entry */
#define MIDI_CTL_MSB_MAIN_VOLUME        0x07 /**< Main Volume */
#define MIDI_CTL_MSB_BALANCE            0x08 /**< Balance */
#define MIDI_CTL_MSB_PAN                0x0A /**< Panpot */
#define MIDI_CTL_MSB_EXPRESSION         0x0B /**< Expression */
#define MIDI_CTL_MSB_EFFECT1            0x0C /**< Effect1 */
#define MIDI_CTL_MSB_EFFECT2            0x0D /**< Effect2 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE1   0x10 /**< General Purpose 1 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE2   0x11 /**< General Purpose 2 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE3   0x12 /**< General Purpose 3 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE4   0x13 /**< General Purpose 4 */
#define MIDI_CTL_LSB_BANK               0x20 /**< Bank Selection */
#define MIDI_CTL_LSB_MODWHEEL           0x21 /**< Modulation */
#define MIDI_CTL_LSB_BREATH             0x22 /**< Breath */
#define MIDI_CTL_LSB_FOOT               0x24 /**< Foot */
#define MIDI_CTL_LSB_PORTAMENTO_TIME    0x25 /**< Portamento Time */
#define MIDI_CTL_LSB_DATA_ENTRY         0x26 /**< Data Entry */
#define MIDI_CTL_LSB_MAIN_VOLUME        0x27 /**< Main Volume */
#define MIDI_CTL_LSB_BALANCE            0x28 /**< Balance */
#define MIDI_CTL_LSB_PAN                0x2A /**< Panpot */
#define MIDI_CTL_LSB_EXPRESSION         0x2B /**< Expression */
#define MIDI_CTL_LSB_EFFECT1            0x2C /**< Effect1 */
#define MIDI_CTL_LSB_EFFECT2            0x2D /**< Effect2 */
#define MIDI_CTL_LSB_GENERAL_PURPOSE1   0x30 /**< General Purpose 1 */
#define MIDI_CTL_LSB_GENERAL_PURPOSE2   0x31 /**< General Purpose 2 */
#define MIDI_CTL_LSB_GENERAL_PURPOSE3   0x32 /**< General Purpose 3 */
#define MIDI_CTL_LSB_GENERAL_PURPOSE4   0x33 /**< General Purpose 4 */
#define MIDI_CTL_SUSTAIN                0x40 /**< Sustain Pedal */
#define MIDI_CTL_PORTAMENTO             0x41 /**< Portamento */
#define MIDI_CTL_SOSTENUTO              0x42 /**< Sostenuto */
#define MIDI_CTL_SOFT_PEDAL             0x43 /**< Soft Pedal */
#define MIDI_CTL_LEGATO_FOOTSWITCH      0x44 /**< Legato Foot Switch */
#define MIDI_CTL_HOLD2                  0x45 /**< Hold2 */
#define MIDI_CTL_SC1_SOUND_VARIATION    0x46 /**< SC1 Sound Variation */
#define MIDI_CTL_SC2_TIMBRE             0x47 /**< SC2 Timbre */
#define MIDI_CTL_SC3_RELEASE_TIME       0x48 /**< SC3 Release Time */
#define MIDI_CTL_SC4_ATTACK_TIME        0x49 /**< SC4 Attack Time */
#define MIDI_CTL_SC5_BRIGHTNESS         0x4A /**< SC5 Brightness */
#define MIDI_CTL_SC6                    0x4B /**< SC6 */
#define MIDI_CTL_SC7                    0x4C /**< SC7 */
#define MIDI_CTL_SC8                    0x4D /**< SC8 */
#define MIDI_CTL_SC9                    0x4E /**< SC9 */
#define MIDI_CTL_SC10                   0x4F /**< SC10 */
#define MIDI_CTL_GENERAL_PURPOSE5       0x50 /**< General Purpose 5 */
#define MIDI_CTL_GENERAL_PURPOSE6       0x51 /**< General Purpose 6 */
#define MIDI_CTL_GENERAL_PURPOSE7       0x52 /**< General Purpose 7 */
#define MIDI_CTL_GENERAL_PURPOSE8       0x53 /**< General Purpose 8 */
#define MIDI_CTL_PORTAMENTO_CONTROL     0x54 /**< Portamento Control */
#define MIDI_CTL_E1_REVERB_DEPTH        0x5B /**< E1 Reverb Depth */
#define MIDI_CTL_E2_TREMOLO_DEPTH       0x5C /**< E2 Tremolo Depth */
#define MIDI_CTL_E3_CHORUS_DEPTH        0x5D /**< E3 Chorus Depth */
#define MIDI_CTL_E4_DETUNE_DEPTH        0x5E /**< E4 Detune Depth */
#define MIDI_CTL_E5_PHASER_DEPTH        0x5F /**< E5 Phaser Depth */
#define MIDI_CTL_DATA_INCREMENT         0x60 /**< Data Increment */
#define MIDI_CTL_DATA_DECREMENT         0x61 /**< Data Decrement */
#define MIDI_CTL_NONREG_PARM_NUM_LSB    0x62 /**< Non-registered Parameter Number */
#define MIDI_CTL_NONREG_PARM_NUM_MSB    0x63 /**< Non-registered Parameter Number */
#define MIDI_CTL_REGIST_PARM_NUM_LSB    0x64 /**< Registered Parameter Number */
#define MIDI_CTL_REGIST_PARM_NUM_MSB    0x65 /**< Registered Parameter Number */
#define MIDI_CTL_ALL_SOUNDS_OFF         0x78 /**< All Sounds Off */
#define MIDI_CTL_RESET_CONTROLLERS      0x79 /**< Reset Controllers */
#define MIDI_CTL_LOCAL_CONTROL_SWITCH   0x7A /**< Local Control Switch */
#define MIDI_CTL_ALL_NOTES_OFF          0x7B /**< All Notes Off */
#define MIDI_CTL_OMNI_OFF               0x7C /**< Omni Off */
#define MIDI_CTL_OMNI_ON                0x7D /**< Omni On */
#define MIDI_CTL_MONO1                  0x7E /**< Mono1 */
#define MIDI_CTL_MONO2                  0x7F /**< Mono2 */

// Commands
#define MIDI_CMD_NOTE_OFF               0x80 /**< Note Off */
#define MIDI_CMD_NOTE_ON                0x90 /**< Note On */
#define MIDI_CMD_NOTE_PRESSURE          0xA0 /**< Key Pressure */
#define MIDI_CMD_CONTROL                0xB0 /**< Control Change */
#define MIDI_CMD_PGM_CHANGE             0xC0 /**< Program Change */
#define MIDI_CMD_CHANNEL_PRESSURE       0xD0 /**< Channel Pressure */
#define MIDI_CMD_BENDER                 0xE0 /**< Pitch Bender */
#define MIDI_CMD_COMMON_SYSEX           0xF0 /**< Sysex (System Exclusive) Begin */
#define MIDI_CMD_COMMON_MTC_QUARTER     0xF1 /**< MTC Quarter Frame */
#define MIDI_CMD_COMMON_SONG_POS        0xF2 /**< Song Position */
#define MIDI_CMD_COMMON_SONG_SELECT     0xF3 /**< Song Select */
#define MIDI_CMD_COMMON_TUNE_REQUEST    0xF6 /**< Tune Request */
#define MIDI_CMD_COMMON_SYSEX_END       0xF7 /**< End of Sysex */
#define MIDI_CMD_COMMON_CLOCK           0xF8 /**< Clock */
#define MIDI_CMD_COMMON_TICK            0xF9 /**< Tick */
#define MIDI_CMD_COMMON_START           0xFA /**< Start */
#define MIDI_CMD_COMMON_CONTINUE        0xFB /**< Continue */
#define MIDI_CMD_COMMON_STOP            0xFC /**< Stop */
#define MIDI_CMD_COMMON_SENSING         0xFE /**< Active Sensing */
#define MIDI_CMD_COMMON_RESET           0xFF /**< Reset */

//@}


/** \} */

#endif /* EVORAL_MIDI_EVENTS_H */
