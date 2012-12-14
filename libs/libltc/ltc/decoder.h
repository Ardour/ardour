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
#ifndef SAMPLE_CENTER // also defined in encoder.h
#define SAMPLE_CENTER 128 // unsigned 8 bit.
#endif

struct LTCDecoder {
	LTCFrameExt* queue;
	int queue_len;
	int queue_read_off;
	int queue_write_off;

	unsigned char biphase_state;
	unsigned char biphase_prev;
	unsigned char snd_to_biphase_state;
	int snd_to_biphase_cnt;		///< counts the samples in the current period
	int snd_to_biphase_lmt;	///< specifies when a state-change is considered biphase-clock or 2*biphase-clock
	double snd_to_biphase_period;	///< track length of a period - used to set snd_to_biphase_lmt

	ltcsnd_sample_t snd_to_biphase_min;
	ltcsnd_sample_t snd_to_biphase_max;

	unsigned short decoder_sync_word;
	LTCFrame ltc_frame;
	int bit_cnt;

	ltc_off_t frame_start_off;
	ltc_off_t frame_start_prev;

	float biphase_tics[LTC_FRAME_BIT_COUNT];
	int biphase_tic;
};


void decode_ltc(LTCDecoder *d, ltcsnd_sample_t *sound, size_t size, ltc_off_t posinfo);
