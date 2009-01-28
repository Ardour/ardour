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

#ifndef __midievents_h__
#define __midievents_h__

/**
 * \defgroup midi MIDI Definitions
 * MIDI command and controller number definitions.
 * \{
 */

// Commands:

#define MIDI_CMD_NOTE_OFF               0x80 /**< note off */
#define MIDI_CMD_NOTE_ON                0x90 /**< note on */
#define MIDI_CMD_NOTE_PRESSURE          0xA0 /**< key pressure */
#define MIDI_CMD_CONTROL                0xB0 /**< control change */
#define MIDI_CMD_PGM_CHANGE             0xC0 /**< program change */
#define MIDI_CMD_CHANNEL_PRESSURE       0xD0 /**< channel pressure */
#define MIDI_CMD_BENDER                 0xE0 /**< pitch bender */

#define MIDI_CMD_COMMON_SYSEX           0xF0 /**< sysex (system exclusive) begin */
#define MIDI_CMD_COMMON_MTC_QUARTER     0xF1 /**< MTC quarter frame */
#define MIDI_CMD_COMMON_SONG_POS        0xF2 /**< song position */
#define MIDI_CMD_COMMON_SONG_SELECT     0xF3 /**< song select */
#define MIDI_CMD_COMMON_TUNE_REQUEST    0xF6 /**< tune request */
#define MIDI_CMD_COMMON_SYSEX_END       0xF7 /**< end of sysex */
#define MIDI_CMD_COMMON_CLOCK           0xF8 /**< clock */
#define MIDI_CMD_COMMON_TICK            0xF9 /**< tick */
#define MIDI_CMD_COMMON_START           0xFA /**< start */
#define MIDI_CMD_COMMON_CONTINUE        0xFB /**< continue */
#define MIDI_CMD_COMMON_STOP            0xFC /**< stop */
#define MIDI_CMD_COMMON_SENSING         0xFE /**< active sensing */
#define MIDI_CMD_COMMON_RESET           0xFF /**< reset */


// Controllers:

#define MIDI_CTL_MSB_BANK               0x00 /**< Bank selection */
#define MIDI_CTL_MSB_MODWHEEL           0x01 /**< Modulation */
#define MIDI_CTL_MSB_BREATH             0x02 /**< Breath */
#define MIDI_CTL_MSB_FOOT               0x04 /**< Foot */
#define MIDI_CTL_MSB_PORTAMENTO_TIME    0x05 /**< Portamento time */
#define MIDI_CTL_MSB_DATA_ENTRY         0x06 /**< Data entry */
#define MIDI_CTL_MSB_MAIN_VOLUME        0x07 /**< Main volume */
#define MIDI_CTL_MSB_BALANCE            0x08 /**< Balance */
#define MIDI_CTL_MSB_PAN                0x0A /**< Panpot */
#define MIDI_CTL_MSB_EXPRESSION         0x0B /**< Expression */
#define MIDI_CTL_MSB_EFFECT1            0x0C /**< Effect1 */
#define MIDI_CTL_MSB_EFFECT2            0x0D /**< Effect2 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE1   0x10 /**< General purpose 1 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE2   0x11 /**< General purpose 2 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE3   0x12 /**< General purpose 3 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE4   0x13 /**< General purpose 4 */
#define MIDI_CTL_LSB_BANK               0x20 /**< Bank selection */
#define MIDI_CTL_LSB_MODWHEEL           0x21 /**< Modulation */
#define MIDI_CTL_LSB_BREATH             0x22 /**< Breath */
#define MIDI_CTL_LSB_FOOT               0x24 /**< Foot */
#define MIDI_CTL_LSB_PORTAMENTO_TIME    0x25 /**< Portamento time */
#define MIDI_CTL_LSB_DATA_ENTRY         0x26 /**< Data entry */
#define MIDI_CTL_LSB_MAIN_VOLUME        0x27 /**< Main volume */
#define MIDI_CTL_LSB_BALANCE            0x28 /**< Balance */
#define MIDI_CTL_LSB_PAN                0x2A /**< Panpot */
#define MIDI_CTL_LSB_EXPRESSION         0x2B /**< Expression */
#define MIDI_CTL_LSB_EFFECT1            0x2C /**< Effect1 */
#define MIDI_CTL_LSB_EFFECT2            0x2D /**< Effect2 */
#define MIDI_CTL_LSB_GENERAL_PURPOSE1   0x30 /**< General purpose 1 */
#define MIDI_CTL_LSB_GENERAL_PURPOSE2   0x31 /**< General purpose 2 */
#define MIDI_CTL_LSB_GENERAL_PURPOSE3   0x32 /**< General purpose 3 */
#define MIDI_CTL_LSB_GENERAL_PURPOSE4   0x33 /**< General purpose 4 */
#define MIDI_CTL_SUSTAIN                0x40 /**< Sustain pedal */
#define MIDI_CTL_PORTAMENTO             0x41 /**< Portamento */
#define MIDI_CTL_SOSTENUTO              0x42 /**< Sostenuto */
#define MIDI_CTL_SUSTENUTO              0x42 /**< Sostenuto (a typo in the older version) */
#define MIDI_CTL_SOFT_PEDAL             0x43 /**< Soft pedal */
#define MIDI_CTL_LEGATO_FOOTSWITCH      0x44 /**< Legato foot switch */
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
#define MIDI_CTL_GENERAL_PURPOSE5       0x50 /**< General purpose 5 */
#define MIDI_CTL_GENERAL_PURPOSE6       0x51 /**< General purpose 6 */
#define MIDI_CTL_GENERAL_PURPOSE7       0x52 /**< General purpose 7 */
#define MIDI_CTL_GENERAL_PURPOSE8       0x53 /**< General purpose 8 */
#define MIDI_CTL_PORTAMENTO_CONTROL     0x54 /**< Portamento control */
#define MIDI_CTL_E1_REVERB_DEPTH        0x5B /**< E1 Reverb Depth */
#define MIDI_CTL_E2_TREMOLO_DEPTH       0x5C /**< E2 Tremolo Depth */
#define MIDI_CTL_E3_CHORUS_DEPTH        0x5D /**< E3 Chorus Depth */
#define MIDI_CTL_E4_DETUNE_DEPTH        0x5E /**< E4 Detune Depth */
#define MIDI_CTL_E5_PHASER_DEPTH        0x5F /**< E5 Phaser Depth */
#define MIDI_CTL_DATA_INCREMENT         0x60 /**< Data Increment */
#define MIDI_CTL_DATA_DECREMENT         0x61 /**< Data Decrement */
#define MIDI_CTL_NONREG_PARM_NUM_LSB    0x62 /**< Non-registered parameter number */
#define MIDI_CTL_NONREG_PARM_NUM_MSB    0x63 /**< Non-registered parameter number */
#define MIDI_CTL_REGIST_PARM_NUM_LSB    0x64 /**< Registered parameter number */
#define MIDI_CTL_REGIST_PARM_NUM_MSB    0x65 /**< Registered parameter number */
#define MIDI_CTL_ALL_SOUNDS_OFF         0x78 /**< All sounds off */
#define MIDI_CTL_RESET_CONTROLLERS      0x79 /**< Reset Controllers */
#define MIDI_CTL_LOCAL_CONTROL_SWITCH   0x7A /**< Local control switch */
#define MIDI_CTL_ALL_NOTES_OFF          0x7B /**< All notes off */
#define MIDI_CTL_OMNI_OFF               0x7C /**< Omni off */
#define MIDI_CTL_OMNI_ON                0x7D /**< Omni on */
#define MIDI_CTL_MONO                  0x7E /**< Monophonic mode */
#define MIDI_CTL_POLY                  0x7F /**< Polyphonic mode */
//@}


/** \} */

#endif /* __midievents_h__ */
