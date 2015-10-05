/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

#ifndef __libpbd_md5_h__
#define __libpbd_md5_h__

#include <string.h>
#include <stdint.h>

#include "pbd/libpbd_visibility.h"

class LIBPBD_API MD5
{
    public:
	MD5();

	// an MD5 digest is a 16-byte number (32 hex digits)
	uint8_t digestRaw[16] ;

	// This version of the digest is actually
	// a "printf'd" version of the digest.
	char digestChars[33] ;

	void   writeToString ();
	char*  digestFile (char *filename);
	char*  digestMemory (uint8_t const * memchunk, size_t len);
	char*  digestString (char const *string);

    private:
	struct context_t {
		uint32_t state[4];       /* state (ABCD) */
		uint32_t count[2];       /* number of bits, modulo 2^64 (lsb first) */
		uint8_t buffer[64]; /* input buffer */
	};

	context_t context;

	void Init ();
	void Transform (uint32_t state[4], uint8_t const * block);
	void Encode (uint8_t *output, uint32_t const *input, size_t len);
	void Decode (uint32_t *output, uint8_t const * input, size_t len);
	void Update (uint8_t const *input, size_t inputLen);
	void Final ();

};

#endif /* __libpbd_md5_h__ */
