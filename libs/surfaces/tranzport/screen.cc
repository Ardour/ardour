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

#include <cstring>
#include <tranzport_control_protocol.h>
#include <cstring>

void
TranzportControlProtocol::screen_clear ()
{
	const char *blank = "                    ";
	print(0,0,blank);
	print(1,0,blank);
}

void TranzportControlProtocol::screen_invalidate ()
{
	screen_invalid.set();
	for(int row = 0; row < ROWS; row++) {
		for(int col = 0; col < COLUMNS; col++) {
			screen_current[row][col] = 0x7f;
			screen_pending[row][col] = ' ';
			screen_flash[row][col] = ' ';
		}
	}
}

void TranzportControlProtocol::screen_validate ()
{
}

void TranzportControlProtocol::screen_init ()
{
	screen_invalidate();
}

// FIXME: Switch to a column oriented flush to make the redraw of the
// meters look better

int
TranzportControlProtocol::screen_flush ()
{
	int cell = 0, row=0, col_base, pending = 0;
	const unsigned long CELL_BITS = 0x0F;
	if ( _device_status == STATUS_OFFLINE) { return (-1); }

	std::bitset<ROWS*COLUMNS> mask(CELL_BITS);
	std::bitset<ROWS*COLUMNS> imask(CELL_BITS);
	for(cell = 0; cell < 10 && pending == 0; cell++) {
		mask = imask << (cell*4);
		if((screen_invalid & mask).any()) {
			/* something in this cell is different, so dump the cell to the device. */
#if DEBUG_TRANZPORT_SCREEN
			printf("MASK   : %s\n", mask.to_string().c_str());
#endif
			if(cell > 4) { row = 1; } else { row = 0; }
			col_base = (cell*4)%COLUMNS;

			uint8_t cmd[8];
			cmd[0] = 0x00;
			cmd[1] = 0x01;
			cmd[2] = cell;
			cmd[3] = screen_pending[row][col_base];
			cmd[4] = screen_pending[row][col_base+1];
			cmd[5] = screen_pending[row][col_base+2];
			cmd[6] = screen_pending[row][col_base+3];
			cmd[7] = 0x00;

			if((pending = lcd_write(cmd)) == 0) {
				/* successful write: copy to current cached display */
				screen_invalid &= mask.flip();
				memcpy (&screen_current[row][col_base], &screen_pending[row][col_base], 4);
			}
		}
	}
	return pending;
}

