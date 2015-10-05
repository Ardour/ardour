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

#include <tranzport_control_protocol.h>

// doing these functions made me realize that screen_invalid should be lcd_isdamaged FIXME soon

bool TranzportControlProtocol::lcd_damage()
{
	screen_invalidate();
	return true;
}

bool TranzportControlProtocol::lcd_damage (int row, int col, int length)
{
	std::bitset<ROWS*COLUMNS> mask1(0);
	// there's an intrinsic to do this fast, darn it, or I'm just sleepy
	for (int i = 0; i < length; i++) { mask1[i] = 1; }
	std::bitset<ROWS*COLUMNS> mask(mask1 << (row*COLUMNS+col));
	screen_invalid |= mask;
	return true;
}

// Still working on the layering, arguably screen_invalid should be lcd_invalid
// or vice versa

bool TranzportControlProtocol::lcd_isdamaged ()
{
	if(screen_invalid.any()) {
#if DEBUG_TRANZPORT > 5
		printf("LCD is damaged somewhere, should redraw it\n");
#endif
		return true;
	}
	return false;
}

bool TranzportControlProtocol::lcd_isdamaged (int row, int col, int length)
{
	// there's an intrinsic to do this fast, darn it
	std::bitset<ROWS*COLUMNS> mask1(0);
	for (int i = 0; i < length; i++) { mask1[i] = 1; }
	std::bitset<ROWS*COLUMNS> mask(mask1 << (row*COLUMNS+col));
	mask &= screen_invalid;
	if(mask.any()) {
#if DEBUG_TRANZPORT > 5
		printf("row: %d,col: %d is damaged, should redraw it\n", row,col);
#endif
		return true;
	}
	return false;
}

// lcd_clear would be a separate function for a smart display
// here it does nothing, but for the sake of completeness it should
// probably write the lcd, and while I'm on the topic it should probably
// take a row, col, length argument....

void
TranzportControlProtocol::lcd_clear ()
{

}

// These lcd commands are not universally used yet and may drop out of the api

int
TranzportControlProtocol::lcd_flush ()
{
	return 0;
}

int
TranzportControlProtocol::lcd_write(uint8_t* cmd, uint32_t timeout_override)
{
	int result;
#if (DEBUG_TRANZPORT_SCREEN > 0)
	printf("VALID  : %s\n", (screen_invalid.to_string()).c_str());
#endif
	if ((result = write(cmd,timeout_override))) {
#if DEBUG_TRANZPORT > 4
		printf("usb screen update failed for some reason... why? \nresult, cmd and data were %d %02x %02x %02x %02x %02x %02x %02x %02x\n",
		       result, cmd[0],cmd[1],cmd[2], cmd[3], cmd[4], cmd[5],cmd[6],cmd[7]);
#endif
	}
	return result;
}

void
TranzportControlProtocol::lcd_fill (uint8_t fill_char)
{
}

void
TranzportControlProtocol::lcd_print (int row, int col, const char* text)
{
	print(row,col,text);
}

void TranzportControlProtocol::lcd_print_noretry (int row, int col, const char* text)
{
	print(row,col,text);
}
