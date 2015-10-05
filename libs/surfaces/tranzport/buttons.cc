/*
 *   Copyright (C) 2006 Paul Davis
 *   Copyright (C) 2007 Michael Taht
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   */

#include "tranzport_control_protocol.h"

#define TRANZPORT_BUTTON_HANDLER(callback, button_arg) if (button_changes & button_arg) { \
		if (buttonmask & button_arg) {				\
			callback##_press (buttonmask&ButtonShift); } else { callback##_release (buttonmask&ButtonShift); } }

int
TranzportControlProtocol::process (uint8_t* buf)
{

	uint32_t this_button_mask;
	uint32_t button_changes;

	_device_status = buf[1];

#if DEBUG_TRANZPORT > 10
	// Perhaps the device can go offline due to flow control, print command bits to see if we have anything interesting
	if(_device_status == STATUS_ONLINE) {
		printf("ONLINE   : %02x %02x %02x %02x %02x %02x %02x %02x\n",
		       buf[0],buf[1],buf[2], buf[3], buf[4], buf[5],buf[6],buf[7]);
	}
	if(_device_status == STATUS_OFFLINE) {
		printf("OFFLINE  : %02x %02x %02x %02x %02x %02x %02x %02x\n",
		       buf[0],buf[1],buf[2], buf[3], buf[4], buf[5],buf[6],buf[7]);
	}

	if(_device_status != STATUS_OK) { return 1; }

#endif


	this_button_mask = 0;
	this_button_mask |= buf[2] << 24;
	this_button_mask |= buf[3] << 16;
	this_button_mask |= buf[4] << 8;
	this_button_mask |= buf[5];
	_datawheel = buf[6];

#if DEBUG_TRANZPORT_STATE > 1
	// Is the state machine incomplete?
	const unsigned int knownstates = 0x00004000|0x00008000|
		0x04000000|    0x40000000|    0x00040000|    0x00400000|
		0x00000400|    0x80000000|    0x02000000|    0x20000000|
		0x00800000|    0x00080000|    0x00020000|    0x00200000|
		0x00000200|    0x01000000|    0x10000000|    0x00010000|
		0x00100000|    0x00000100|    0x08000000|    0x00001000;

	std::bitset<32> bi(knownstates);
	std::bitset<32> vi(this_button_mask);

	//  if an bi & vi == vi the same - it's a valid set

	if(vi != (bi & vi)) {
		printf("UNKNOWN STATE: %s also, datawheel= %d\n", vi.to_string().c_str(), _datawheel);
	}
#endif

	button_changes = (this_button_mask ^ buttonmask);
	buttonmask = this_button_mask;

	if (_datawheel) {
		datawheel ();
	}

	// SHIFT + STOP + PLAY for bling mode?
	// if (button_changes & ButtonPlay & ButtonStop) {
	// bling_mode_toggle();
	// } or something like that

	TRANZPORT_BUTTON_HANDLER(button_event_battery,ButtonBattery);
	TRANZPORT_BUTTON_HANDLER(button_event_backlight,ButtonBacklight);
	TRANZPORT_BUTTON_HANDLER(button_event_trackleft,ButtonTrackLeft);
	TRANZPORT_BUTTON_HANDLER(button_event_trackright,ButtonTrackRight);
	TRANZPORT_BUTTON_HANDLER(button_event_trackrec,ButtonTrackRec);
	TRANZPORT_BUTTON_HANDLER(button_event_trackmute,ButtonTrackMute);
	TRANZPORT_BUTTON_HANDLER(button_event_tracksolo,ButtonTrackSolo);
	TRANZPORT_BUTTON_HANDLER(button_event_undo,ButtonUndo);
	TRANZPORT_BUTTON_HANDLER(button_event_in,ButtonIn);
	TRANZPORT_BUTTON_HANDLER(button_event_out,ButtonOut);
	TRANZPORT_BUTTON_HANDLER(button_event_punch,ButtonPunch);
	TRANZPORT_BUTTON_HANDLER(button_event_loop,ButtonLoop);
	TRANZPORT_BUTTON_HANDLER(button_event_prev,ButtonPrev);
	TRANZPORT_BUTTON_HANDLER(button_event_add,ButtonAdd);
	TRANZPORT_BUTTON_HANDLER(button_event_next,ButtonNext);
	TRANZPORT_BUTTON_HANDLER(button_event_rewind,ButtonRewind);
	TRANZPORT_BUTTON_HANDLER(button_event_fastforward,ButtonFastForward);
	TRANZPORT_BUTTON_HANDLER(button_event_stop,ButtonStop);
	TRANZPORT_BUTTON_HANDLER(button_event_play,ButtonPlay);
	TRANZPORT_BUTTON_HANDLER(button_event_record,ButtonRecord);
	TRANZPORT_BUTTON_HANDLER(button_event_footswitch,ButtonFootswitch);
	return 0;
}

