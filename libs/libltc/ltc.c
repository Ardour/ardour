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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ltc/ltc.h"
#include "ltc/decoder.h"
#include "ltc/encoder.h"

/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * Decoder
 */

LTCDecoder* ltc_decoder_create(int apv, int queue_len) {
	LTCDecoder* d = (LTCDecoder*) calloc(1, sizeof(LTCDecoder));
	if (!d) return NULL;

	d->queue_len = queue_len;
	d->queue = (LTCFrameExt*) calloc(d->queue_len, sizeof(LTCFrameExt));
	if (!d->queue) {
		free(d);
		return NULL;
	}
	d->biphase_state = 1;
	d->snd_to_biphase_period = apv / 80;
	d->snd_to_biphase_lmt = (d->snd_to_biphase_period * 3) / 4;

	d->snd_to_biphase_min = SAMPLE_CENTER;
	d->snd_to_biphase_max = SAMPLE_CENTER;
	d->frame_start_prev = -1;
	d->biphase_tic = 0;

	return d;
}

int ltc_decoder_free(LTCDecoder *d) {
	if (!d) return 1;
	if (d->queue) free(d->queue);
	free(d);

	return 0;
}

void ltc_decoder_write(LTCDecoder *d, ltcsnd_sample_t *buf, size_t size, ltc_off_t posinfo) {
	decode_ltc(d, buf, size, posinfo);
}

#define LTCWRITE_TEMPLATE(FN, FORMAT, CONV) \
void ltc_decoder_write_ ## FN (LTCDecoder *d, FORMAT *buf, size_t size, ltc_off_t posinfo) { \
	ltcsnd_sample_t tmp[1024]; \
	size_t remain = size; \
	while (remain > 0) { \
		int c = (remain > 1024) ? 1024 : remain; \
		int i; \
		for (i=0; i<c; i++) { \
			tmp[i] = CONV; \
		} \
		decode_ltc(d, tmp, c, posinfo + (ltc_off_t)c); \
		remain -= c; \
	} \
}

LTCWRITE_TEMPLATE(float, float, 128 + (buf[i] * 127.0))
LTCWRITE_TEMPLATE(s16, short, 128 + (buf[i] >> 8))
LTCWRITE_TEMPLATE(u16, short, (buf[i] >> 8))

int ltc_decoder_read(LTCDecoder* d, LTCFrameExt* frame) {
	if (!frame) return -1;
	if (d->queue_read_off != d->queue_write_off) {
		memcpy(frame, &d->queue[d->queue_read_off], sizeof(LTCFrameExt));
		d->queue_read_off++;
		if (d->queue_read_off == d->queue_len)
			d->queue_read_off = 0;
		return 1;
	}
	return 0;
}

void ltc_decoder_queue_flush(LTCDecoder* d) {
	while (d->queue_read_off != d->queue_write_off) {
		d->queue_read_off++;
		if (d->queue_read_off == d->queue_len)
			d->queue_read_off = 0;
	}
}

int ltc_decoder_queue_length(LTCDecoder* d) {
	return (d->queue_write_off - d->queue_read_off + d->queue_len) % d->queue_len;
}

/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * Encoder
 */

LTCEncoder* ltc_encoder_create(double sample_rate, double fps, enum LTC_TV_STANDARD standard, int flags) {
	if (sample_rate < 1)
		return NULL;

	LTCEncoder* e = (LTCEncoder*) calloc(1, sizeof(LTCEncoder));
	if (!e)
		return NULL;

	/*-3.0 dBFS default */
	e->enc_lo = 38;
	e->enc_hi = 218;

	e->bufsize = 1 + ceil(sample_rate / fps);
	e->buf = (ltcsnd_sample_t*) calloc(e->bufsize, sizeof(ltcsnd_sample_t));
	if (!e->buf) {
		free(e);
		return NULL;
	}

	ltc_frame_reset(&e->f);
	ltc_encoder_reinit(e, sample_rate, fps, standard, flags);
	return e;
}

void ltc_encoder_free(LTCEncoder *e) {
	if (!e) return;
	if (e->buf) free(e->buf);
	free(e);
}

int ltc_encoder_reinit(LTCEncoder *e, double sample_rate, double fps, enum LTC_TV_STANDARD standard, int flags) {
	if (sample_rate < 1)
		return -1;

	size_t bufsize = 1 + ceil(sample_rate / fps);
	if (bufsize > e->bufsize) {
		return -1;
	}

	e->state = 0;
	e->offset = 0;
	e->sample_rate = sample_rate;
	ltc_encoder_set_filter(e, 40.0);
	e->fps = fps;
	e->flags = flags;
	e->standard = standard;
	e->samples_per_clock = sample_rate / (fps * 80.0);
	e->samples_per_clock_2 = e->samples_per_clock / 2.0;
	e->sample_remainder = 0.5;

	if (flags & LTC_BGF_DONT_TOUCH) {
		e->f.col_frame = 0;
		if (flags&LTC_TC_CLOCK) {
			e->f.binary_group_flag_bit1 = 1;
		} else {
			e->f.binary_group_flag_bit1 = 0;
		}
		switch (standard) {
			case LTC_TV_625_50: /* 25 fps mode */
				e->f.biphase_mark_phase_correction = 0; // BGF0
				e->f.binary_group_flag_bit0 = (flags&LTC_USE_DATE)?1:0; // BGF2
				break;
			default:
				e->f.binary_group_flag_bit0 = 0;
				e->f.binary_group_flag_bit2 = (flags&LTC_USE_DATE)?1:0;
				break;
		}
	}
	if ((flags&LTC_NO_PARITY) == 0) {
		ltc_frame_set_parity(&e->f, standard);
	}

	if (rint(fps*100) == 2997)
		e->f.dfbit = 1;
	else
		e->f.dfbit = 0;
	return 0;
}

void ltc_encoder_reset(LTCEncoder *e) {
	e->state = 0;
	e->sample_remainder = 0.5;
	e->offset = 0;
}

int ltc_encoder_set_volume(LTCEncoder *e, double dBFS) {
	if (dBFS > 0)
		return -1;
	double pp = rint(127.0 * pow(10, dBFS/20.0));
	if (pp < 1 || pp > 127)
		return -1;
	ltcsnd_sample_t diff = ((ltcsnd_sample_t) pp)&0x7f;
	e->enc_lo = SAMPLE_CENTER - diff;
	e->enc_hi = SAMPLE_CENTER + diff;
	return 0;
}

void ltc_encoder_set_filter(LTCEncoder *e, double rise_time) {
	/* low-pass-filter
	 * LTC signal should have a rise time of 40 us +/- 10 us.
	 *
	 * rise-time means from <10% to >90% of the signal.
	 * in each call to addvalues() we start at 50% (SAMPLE_CENTER), so
	 * here we need half-of it.
	 */

	if (rise_time <= 0)
		e->filter_const = 0;
	else
		e->filter_const = 1.0 - exp( -1.0 / (e->sample_rate * rise_time / 2000000.0 / exp(1.0)) );
}

int ltc_encoder_set_bufsize(LTCEncoder *e, double sample_rate, double fps) {
	free (e->buf);
	e->offset = 0;
	e->bufsize = 1 + ceil(sample_rate / fps);
	e->buf = (ltcsnd_sample_t*) calloc(e->bufsize, sizeof(ltcsnd_sample_t));
	if (!e->buf) {
		return -1;
	}
	return 0;
}

int ltc_encoder_encode_byte(LTCEncoder *e, int byte, double speed) {
	return encode_byte(e, byte, speed);
}

void ltc_encoder_encode_frame(LTCEncoder *e) {
	int byte;
	for (byte = 0 ; byte < 10 ; byte++) {
		encode_byte(e, byte, 1.0);
	}
}

void ltc_encoder_get_timecode(LTCEncoder *e, SMPTETimecode *t) {
	ltc_frame_to_time(t, &e->f, e->flags);
}

void ltc_encoder_set_timecode(LTCEncoder *e, SMPTETimecode *t) {
	ltc_time_to_frame(&e->f, t, e->standard, e->flags);
}

void ltc_encoder_get_frame(LTCEncoder *e, LTCFrame *f) {
	memcpy(f, &e->f, sizeof(LTCFrame));
}

void ltc_encoder_set_frame(LTCEncoder *e, LTCFrame *f) {
	memcpy(&e->f, f, sizeof(LTCFrame));
}

int ltc_encoder_inc_timecode(LTCEncoder *e) {
	return ltc_frame_increment (&e->f, rint(e->fps), e->standard, e->flags);
}

int ltc_encoder_dec_timecode(LTCEncoder *e) {
	return ltc_frame_decrement (&e->f, rint(e->fps), e->standard, e->flags);
}

size_t ltc_encoder_get_buffersize(LTCEncoder *e) {
	return(e->bufsize);
}

void ltc_encoder_buffer_flush(LTCEncoder *e) {
	e->offset = 0;
}

ltcsnd_sample_t *ltc_encoder_get_bufptr(LTCEncoder *e, int *size, int flush) {
	if (size) *size = e->offset;
	if (flush) e->offset = 0;
	return e->buf;
}

int ltc_encoder_get_buffer(LTCEncoder *e, ltcsnd_sample_t *buf) {
	const int len = e->offset;
	memcpy(buf, e->buf, len * sizeof(ltcsnd_sample_t) );
	e->offset = 0;
	return(len);
}

void ltc_frame_set_parity(LTCFrame *frame, enum LTC_TV_STANDARD standard) {
	int i;
	unsigned char p = 0;

	if (standard != LTC_TV_625_50) { /* 30fps, 24fps */
		frame->biphase_mark_phase_correction = 0;
	} else { /* 25fps */
		frame->binary_group_flag_bit2 = 0;
	}

	for (i=0; i < LTC_FRAME_BIT_COUNT / 8; ++i){
		p = p ^ (((unsigned char*)frame)[i]);
	}
#define PRY(BIT) ((p>>BIT)&1)

	if (standard != LTC_TV_625_50) { /* 30fps, 24fps */
		frame->biphase_mark_phase_correction =
			PRY(0)^PRY(1)^PRY(2)^PRY(3)^PRY(4)^PRY(5)^PRY(6)^PRY(7);
	} else { /* 25fps */
		frame->binary_group_flag_bit2 =
			PRY(0)^PRY(1)^PRY(2)^PRY(3)^PRY(4)^PRY(5)^PRY(6)^PRY(7);
	}
}

ltc_off_t ltc_frame_alignment(double samples_per_frame, enum LTC_TV_STANDARD standard) {
	switch (standard) {
		case LTC_TV_525_60:
			return rint(samples_per_frame * 4.0 / 525.0);
		case LTC_TV_625_50:
			return rint(samples_per_frame * 1.0 / 625.0);
		default:
			return 0;
	}
}
