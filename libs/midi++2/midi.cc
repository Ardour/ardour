/*
 * Copyright (C) 1998-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
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

#include <cstring>
#include <cstdlib>
#include "midi++/types.h"

const char *MIDI::controller_names[] = {
	"bank (0)",
	"mod (1)",
	"breath (2)",
	"ctrl 3",
	"foot (4)",
	"port tm (5)",
	"data msb (6)",
	"volume (7)",
	"balance (8)",
	"ctrl 9",
	"pan (10)",
	"express (11)",
	"ctrl 12",
	"ctrl 13",
	"ctrl 14",
	"ctrl 15",
	"gpc 1",
	"gpc 2",
	"gpc 3",
	"gpc 4",
	"ctrl 20",
	"ctrl 21",
	"ctrl 22",
	"ctrl 23",
	"ctrl 24",
	"ctrl 25",
	"ctrl 26",
	"ctrl 27",
	"ctrl 28",
	"ctrl 29",
	"ctrl 30",
	"ctrl 31",
	"lsb 0 (32)",
	"lsb 1 (33)",
	"lsb 2 (34)",
	"lsb 3 (35)",
	"lsb 4 (36)",
	"lsb 5 (37)",
	"lsb 6 (38)",
	"lsb 7 (39)",
	"lsb 8 (40)",
	"lsb 9 (41)",
	"lsb 10 (42)",
	"lsb 11 (43)",
	"lsb 12 (44)",
	"lsb 13 (45)",
	"lsb 14 (46)",
	"lsb 15 (47)",
	"lsb 16 (48)",
	"lsb 17 (49)",
	"lsb 18 (50)",
	"lsb 19 (51)",
	"lsb 20 (52)",
	"lsb 21 (53)",
	"lsb 22 (54)",
	"lsb 23 (55)",
	"lsb 24 (56)",
	"lsb 25 (57)",
	"lsb 26 (58)",
	"lsb 27 (59)",
	"lsb 28 (60)",
	"lsb 29 (61)",
	"lsb 30 (62)",
	"lsb 31 (63)",
	"sustain (64)",
	"portamento (65)",
	"sostenuto (66)",
	"soft ped (67)",
	"ctrl 68",
	"hold 2 (69)",
	"ctrl 70",
	"ctrl 71",
	"ctrl 72",
	"ctrl 73",
	"ctrl 74",
	"ctrl 75",
	"ctrl 76",
	"ctrl 77",
	"ctrl 78",
	"ctrl 79",
	"gpc 5 (80)",
	"gpc 6 (81)",
	"gpc 7 (82)",
	"gpc 8 (83)",
	"ctrl 84",
	"ctrl 85",
	"ctrl 86",
	"ctrl 87",
	"ctrl 88",
	"ctrl 89",
	"ctrl 90",
	"fx dpth (91)",
	"tremolo (92)",
	"chorus (93)",
	"detune (94)",
	"phaser (95)",
	"data inc (96)",
	"data dec (97)",
	"nrpn lsb (98)",
	"nrpn msg (99)",
	"rpn lsb (100)",
	"rpn msb (101)",
	"ctrl 102",
	"ctrl 103",
	"ctrl 104",
	"ctrl 105",
	"ctrl 106",
	"ctrl 107",
	"ctrl 108",
	"ctrl 109",
	"ctrl 110",
	"ctrl 111",
	"ctrl 112",
	"ctrl 113",
	"ctrl 114",
	"ctrl 115",
	"ctrl 116",
	"ctrl 117",
	"ctrl 118",
	"ctrl 119",
	"snd off (120)",
	"rst ctrl (121)",
	"local (122)",
	"notes off (123)",
	"omni off (124)",
	"omni on (125)",
	"mono on (126)",
	"poly on (127)",
	0
};

MIDI::byte
MIDI::decode_controller_name (const char *name)

{
	const char *lparen;
	size_t len;

	if ((lparen = strrchr (name, '(')) != 0) {
		return atoi (lparen+1);
	} else {
		len = strcspn (name, "0123456789");
		return atoi (name+len);
	}
}
