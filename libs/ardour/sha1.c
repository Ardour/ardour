/*
 * Copyright (C) 2012,2015 Robin Gareus <robin@gareus.org>
 *
 * This code is inspired by libcrypt, which was placed
 * in the public domain by Wei Dai and other contributors.
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

#ifndef EXPORT_SHA
#define EXPORT_SHA static
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#if defined(PLATFORM_WINDOWS) && !defined(__LITTLE_ENDIAN__)
#define __LITTLE_ENDIAN__
#endif

#ifdef __BIG_ENDIAN__
# define SHA_BIG_ENDIAN
#elif defined _BIG_ENDIAN
# define SHA_BIG_ENDIAN
#elif defined __BYTE_ORDER__
# if __BYTE_ORDER__ ==  __ORDER_BIG_ENDIAN__
#  define SHA_BIG_ENDIAN
# endif
#elif !defined __LITTLE_ENDIAN__
# include <endian.h> // machine/endian.h
# if (defined __BYTE_ORDER__ && defined __ORDER_BIG_ENDIAN__ && __BYTE_ORDER__ ==  __ORDER_BIG_ENDIAN__)
#  define SHA_BIG_ENDIAN
# endif
#endif

typedef struct {
	uint32_t buffer[16];
	uint32_t state[5];
	uint32_t byteCount;
	uint8_t bufferOffset;
} Sha1Digest;


static inline uint32_t sha1_rol32 (uint32_t number, uint8_t bits) {
	return ((number << bits) | (number >> (32 - bits)));
}

static void sha1_hashBlock (Sha1Digest *s) {
	uint8_t i;
	uint32_t a, b, c, d, e, t;

	a = s->state[0];
	b = s->state[1];
	c = s->state[2];
	d = s->state[3];
	e = s->state[4];

	for (i = 0; i < 80; ++i) {
		if (i >= 16) {
			t = s->buffer[(i + 13) & 15] ^ s->buffer[(i + 8) & 15] ^ s->buffer[(i + 2) & 15] ^ s->buffer[i & 15];
			s->buffer[i & 15] = sha1_rol32 (t, 1);
		}
		if (i < 20) {
			t = (d ^ (b & (c ^ d))) + 0x5a827999;
		} else if (i < 40) {
			t = (b ^ c ^ d) + 0x6ed9eba1;
		} else if (i < 60) {
			t = ((b & c) | (d & (b | c))) + 0x8f1bbcdc;
		} else {
			t = (b ^ c ^ d) + 0xca62c1d6;
		}
		t += sha1_rol32 (a, 5) + e + s->buffer[i & 15];
		e = d;
		d = c;
		c = sha1_rol32 (b, 30);
		b = a;
		a = t;
	}

	s->state[0] += a;
	s->state[1] += b;
	s->state[2] += c;
	s->state[3] += d;
	s->state[4] += e;
}

static void sha1_addUncounted (Sha1Digest *s, const uint8_t data) {
	uint8_t * const b = (uint8_t*) s->buffer;
#ifdef SHA_BIG_ENDIAN
	b[s->bufferOffset] = data;
#else
	b[s->bufferOffset ^ 3] = data;
#endif
	s->bufferOffset++;
	if (s->bufferOffset == 64) {
		sha1_hashBlock (s);
		s->bufferOffset = 0;
	}
}

static void sha1_pad (Sha1Digest *s) {
	// Implement SHA-1 padding (fips180-2 5.1.1)
	// Pad with 0x80 followed by 0x00 until the end of the block
	sha1_addUncounted (s, 0x80);
	while (s->bufferOffset != 56) sha1_addUncounted (s, 0x00);

	// Append length in the last 8 bytes
	sha1_addUncounted (s, 0); // We're only using 32 bit lengths
	sha1_addUncounted (s, 0); // But SHA-1 supports 64 bit lengths
	sha1_addUncounted (s, 0); // So zero pad the top bits
	sha1_addUncounted (s, s->byteCount >> 29); // Shifting to multiply by 8
	sha1_addUncounted (s, s->byteCount >> 21); // as SHA-1 supports bitstreams as well as
	sha1_addUncounted (s, s->byteCount >> 13); // byte.
	sha1_addUncounted (s, s->byteCount >> 5);
	sha1_addUncounted (s, s->byteCount << 3);
}


/*** public functions ***/

EXPORT_SHA void sha1_init (Sha1Digest *s) {
	s->state[0] = 0x67452301;
	s->state[1] = 0xefcdab89;
	s->state[2] = 0x98badcfe;
	s->state[3] = 0x10325476;
	s->state[4] = 0xc3d2e1f0;
	s->byteCount = 0;
	s->bufferOffset = 0;
}

EXPORT_SHA void sha1_writebyte (Sha1Digest *s, const uint8_t data) {
	++s->byteCount;
	sha1_addUncounted (s, data);
}

EXPORT_SHA void sha1_write (Sha1Digest *s, const uint8_t *data, size_t len) {
	for (;len--;) sha1_writebyte (s, (uint8_t) *data++);
}

EXPORT_SHA uint8_t* sha1_result (Sha1Digest *s) {
	// Pad to complete the last block
	sha1_pad (s);

#ifndef SHA_BIG_ENDIAN
	// Swap byte order back
	int i;
	for (i = 0; i < 5; ++i) {
		s->state[i] =
			  (((s->state[i])<<24)& 0xff000000)
			| (((s->state[i])<<8) & 0x00ff0000)
			| (((s->state[i])>>8) & 0x0000ff00)
			| (((s->state[i])>>24)& 0x000000ff);
	}
#endif
	// Return pointer to hash (20 characters)
	return (uint8_t*) s->state;
}

EXPORT_SHA void sha1_result_hash (Sha1Digest *s, char *rv) {
	int i;
	uint8_t* hash = sha1_result (s);
	for (i = 0; i < 20; ++i) {
		sprintf (&rv[2*i], "%02x", hash[i]);
	}
}
