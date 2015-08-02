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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// gcc -Wall -DSELFTEST_SHA1 -o /tmp/sha1test libs/ardour/sha1.c && /tmp/sha1test

#ifndef EXPORT_SHA
#define EXPORT_SHA static
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


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
# if __BYTE_ORDER__ ==  __ORDER_BIG_ENDIAN__
#  define SHA_BIG_ENDIAN
# endif
#endif


#define HASH_LENGTH 20
#define BLOCK_LENGTH 64

typedef struct {
	uint32_t buffer[BLOCK_LENGTH/4];
	uint32_t state[HASH_LENGTH/4];
	uint32_t byteCount;
	uint8_t bufferOffset;
	uint8_t keyBuffer[BLOCK_LENGTH];
	uint8_t innerHash[HASH_LENGTH];
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
	if (s->bufferOffset == BLOCK_LENGTH) {
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


/*** self-test ***/
#ifdef SELFTEST_SHA1
void printHash (Sha1Digest *s) {
	char hash[41];
	sha1_result_hash (s, hash);
	printf ("%s\n", hash);
}

int main (int argc, char **argv) {
	uint32_t a;
	Sha1Digest s;

	// SHA tests
	printf ("Test: FIPS 180-2 C.1 and RFC3174 7.3 TEST1\n");
	printf ("Expect:a9993e364706816aba3e25717850c26c9cd0d89d\n");
	printf ("Result:");
	sha1_init (&s);
	sha1_write (&s, "abc", 3);
	printHash (&s);
	printf ("\n\n");

	printf ("Test: FIPS 180-2 C.2 and RFC3174 7.3 TEST2\n");
	printf ("Expect:84983e441c3bd26ebaae4aa1f95129e5e54670f1\n");
	printf ("Result:");
	sha1_init (&s);
	sha1_write (&s, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56);
	printHash (&s);
	printf ("\n\n");

	printf ("Test: RFC3174 7.3 TEST4\n");
	printf ("Expect:dea356a2cddd90c7a7ecedc5ebb563934f460452\n");
	printf ("Result:");
	sha1_init (&s);
	for (a = 0; a < 80; ++a) sha1_write (&s, "01234567", 8);
	printHash (&s);
	printf ("\n\n");

	printf ("Test: FIPS 180-2 C.3 and RFC3174 7.3 TEST3\n");
	printf ("Expect:34aa973cd4c4daa4f61eeb2bdbad27316534016f\n");
	printf ("Result:");
	sha1_init (&s);
	for (a = 0; a < 1000000; ++a) sha1_writebyte (&s, 'a');
	printHash (&s);

	return 0;
}
#endif /* self-test */
