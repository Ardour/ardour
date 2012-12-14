/*
   libltc - en+decode linear timecode

   Copyright (C) 2006-2012 Robin Gareus <robin@gareus.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.
   If not, see <http://www.gnu.org/licenses/>.
*/

#include "ltc/ltc.h"
#ifndef SAMPLE_CENTER // also defined in decoder.h
#define SAMPLE_CENTER 128 // unsigned 8 bit.
#endif

struct LTCEncoder {
	double fps;
	double sample_rate;
	double filter_const;
	int flags;
	enum LTC_TV_STANDARD standard;
	ltcsnd_sample_t enc_lo, enc_hi;

	size_t offset;
	size_t bufsize;
	ltcsnd_sample_t *buf;

	char state;

	double samples_per_clock;
	double samples_per_clock_2;
	double sample_remainder;

	LTCFrame f;
};

int encode_byte(LTCEncoder *e, int byte, double speed);
