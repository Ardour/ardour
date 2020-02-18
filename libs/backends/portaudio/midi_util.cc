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

#include "midi_util.h"

int get_midi_msg_length (uint8_t status_byte)
{
	// define these with meaningful names
	switch (status_byte & 0xf0) {
	case 0x80:
	case 0x90:
	case 0xa0:
	case 0xb0:
	case 0xe0:
		return 3;
	case 0xc0:
	case 0xd0:
		return 2;
	case 0xf0:
		switch (status_byte) {
		case 0xf0:
			return 0;
		case 0xf1:
		case 0xf3:
			return 2;
		case 0xf2:
			return 3;
		case 0xf4:
		case 0xf5:
		case 0xf7:
		case 0xfd:
			break;
		default:
			return 1;
		}
	}
	return -1;
}
