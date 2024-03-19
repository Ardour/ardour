/*
 * Copyright (C) 2023-2024 Adrien Gesta-Fline
 *
 * This file is part of libAAF.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/RIFFParser.h"

#define debug(...) \
	AAF_LOG (log, NULL, LOG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (log, NULL, LOG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (log, NULL, LOG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

#define BE2LE32(val) \
	(((val >> 24) & 0xff) | ((val << 8) & 0xff0000) | ((val >> 8) & 0xff00) | ((val << 24) & 0xff000000))

#define BE2LE16(val) \
	((val << 8) | (val >> 8))

static uint32_t
beExtended2leUint32 (const unsigned char numx[10]);

int
laaf_riff_writeWavFileHeader (FILE* fp, struct wavFmtChunk* wavFmt, struct wavBextChunk* wavBext, uint32_t audioDataSize, struct aafLog* log)
{
	(void)log;
	size_t filesize = (4 /* WAVE */) + sizeof (struct wavFmtChunk) + ((wavBext) ? sizeof (struct wavBextChunk) : 0) + (8 /*data chunk header*/) + audioDataSize;

	size_t writtenBytes = fwrite ("RIFF", sizeof (unsigned char), 4, fp);

	if (writtenBytes < 4) {
		return -1;
	}

	writtenBytes = fwrite (&filesize, sizeof (uint32_t), 1, fp);

	if (writtenBytes < 1) {
		return -1;
	}

	writtenBytes = fwrite ("WAVE", sizeof (unsigned char), 4, fp);

	if (writtenBytes < 4) {
		return -1;
	}

	wavFmt->ckid[0]           = 'f';
	wavFmt->ckid[1]           = 'm';
	wavFmt->ckid[2]           = 't';
	wavFmt->ckid[3]           = ' ';
	wavFmt->cksz              = sizeof (struct wavFmtChunk) - sizeof (struct riffChunk);
	wavFmt->format_tag        = 1; /* PCM */
	wavFmt->avg_bytes_per_sec = wavFmt->samples_per_sec * wavFmt->channels * wavFmt->bits_per_sample / 8;
	wavFmt->block_align       = wavFmt->channels * (wavFmt->bits_per_sample >> 3);

	writtenBytes = fwrite ((unsigned char*)wavFmt, sizeof (unsigned char), sizeof (struct wavFmtChunk), fp);

	if (writtenBytes < sizeof (struct wavFmtChunk)) {
		return -1;
	}

	if (wavBext) {
		wavBext->ckid[0] = 'b';
		wavBext->ckid[1] = 'e';
		wavBext->ckid[2] = 'x';
		wavBext->ckid[3] = 't';
		wavBext->cksz    = sizeof (struct wavBextChunk) - sizeof (struct riffChunk);
		wavBext->version = 1;

		writtenBytes = fwrite ((unsigned char*)wavBext, sizeof (unsigned char), sizeof (struct wavBextChunk), fp);

		if (writtenBytes < sizeof (struct wavBextChunk)) {
			return -1;
		}
	}

	writtenBytes = fwrite ("data", sizeof (unsigned char), 4, fp);

	if (writtenBytes < 4) {
		return -1;
	}

	writtenBytes = fwrite (&audioDataSize, sizeof (uint32_t), 1, fp);

	if (writtenBytes < 1) {
		return -1;
	}

	return 0;
}

int
laaf_riff_parseAudioFile (struct RIFFAudioFile* RIFFAudioFile, enum RIFF_PARSER_FLAGS flags, size_t (*readerCallback) (unsigned char*, size_t, size_t, void*, void*, void*), void* user1, void* user2, void* user3, struct aafLog* log)
{
	struct riffChunk       chunk;
	struct riffHeaderChunk riff;

	memset (RIFFAudioFile, 0x00, sizeof (struct RIFFAudioFile));

	size_t bytesRead = readerCallback ((unsigned char*)&riff, 0, sizeof (riff), user1, user2, user3);

	if (bytesRead == RIFF_READER_ERROR ||
	    bytesRead < sizeof (riff)) {
		error ("Could not read file header");
		return -1;
	}

	int be = 0; /* big endian */

	if (riff.format[0] == 'A' &&
	    riff.format[1] == 'I' &&
	    riff.format[2] == 'F' &&
	    riff.format[3] == 'F') {
		be        = 1;
		riff.cksz = BE2LE32 (riff.cksz);
	} else if (riff.format[0] == 'A' &&
	           riff.format[1] == 'I' &&
	           riff.format[2] == 'F' &&
	           riff.format[3] == 'C') {
		be        = 1;
		riff.cksz = BE2LE32 (riff.cksz);
	} else if (riff.format[0] == 'W' &&
	           riff.format[1] == 'A' &&
	           riff.format[2] == 'V' &&
	           riff.format[3] == 'E') {
		be = 0;
	} else {
		error ("File is not a valid RIFF/WAVE or RIFF/AIFF : Missing format identifier");
		return -1;
	}

	size_t filesize = riff.cksz + sizeof (chunk);
	size_t pos_sz   = 0;
	size_t pos      = sizeof (struct riffHeaderChunk);

	while (pos < filesize) {
		bytesRead = readerCallback ((unsigned char*)&chunk, pos, sizeof (chunk), user1, user2, user3);

		if (bytesRead == RIFF_READER_ERROR ||
		    bytesRead < sizeof (chunk)) {
			error ("Could not read chunk \"%.4s\" @ %" PRIu64 " (%" PRIu64 " bytes returned)", chunk.ckid, pos, bytesRead);
			break;
		}

		if (be) {
			chunk.cksz = BE2LE32 (chunk.cksz);
		}

		debug ("Got chunk \"%.4s\" (%u bytes) @ %" PRIu64 " (%" PRIu64 " bytes returned)", chunk.ckid, chunk.cksz, pos, bytesRead);

		if (!be) { /* WAVE */

			if (chunk.ckid[0] == 'f' &&
			    chunk.ckid[1] == 'm' &&
			    chunk.ckid[2] == 't' &&
			    chunk.ckid[3] == ' ') {
				struct wavFmtChunk wavFmtChunk;

				bytesRead = readerCallback ((unsigned char*)&wavFmtChunk, pos, sizeof (wavFmtChunk), user1, user2, user3);

				if (bytesRead == RIFF_READER_ERROR ||
				    bytesRead < sizeof (riff)) {
					error ("Could not read chunk \"%.4s\" content @ %" PRIu64 " (%" PRIu64 " bytes returned)", chunk.ckid, pos, bytesRead);
					break;
				}

				RIFFAudioFile->channels   = wavFmtChunk.channels;
				RIFFAudioFile->sampleSize = wavFmtChunk.bits_per_sample;
				RIFFAudioFile->sampleRate = wavFmtChunk.samples_per_sec;

				if (flags & RIFF_PARSE_ONLY_HEADER) {
					return 0;
				}
			} else if (chunk.ckid[0] == 'd' &&
			           chunk.ckid[1] == 'a' &&
			           chunk.ckid[2] == 't' &&
			           chunk.ckid[3] == 'a') {
				if (RIFFAudioFile->channels > 0 && RIFFAudioFile->sampleSize > 0) {
					RIFFAudioFile->sampleCount = chunk.cksz / RIFFAudioFile->channels / (RIFFAudioFile->sampleSize / 8);
				}

				RIFFAudioFile->pcm_audio_start_offset = (pos + sizeof (struct riffChunk));

				if (flags & RIFF_PARSE_AAF_SUMMARY) {
					return 0;
				}
			}
		} else { /* AIFF */

			if (chunk.ckid[0] == 'C' &&
			    chunk.ckid[1] == 'O' &&
			    chunk.ckid[2] == 'M' &&
			    chunk.ckid[3] == 'M') {
				struct aiffCOMMChunk aiffCOMMChunk;

				bytesRead = readerCallback ((unsigned char*)&aiffCOMMChunk, pos, sizeof (aiffCOMMChunk), user1, user2, user3);

				if (bytesRead == RIFF_READER_ERROR ||
				    bytesRead < sizeof (riff)) {
					error ("Could not read chunk \"%.4s\" content @ %" PRIu64 " (%" PRIu64 " bytes returned)", chunk.ckid, pos, bytesRead);
					break;
				}

				RIFFAudioFile->channels    = BE2LE16 (aiffCOMMChunk.numChannels);
				RIFFAudioFile->sampleSize  = BE2LE16 (aiffCOMMChunk.sampleSize);
				RIFFAudioFile->sampleRate  = beExtended2leUint32 (aiffCOMMChunk.sampleRate);
				RIFFAudioFile->sampleCount = BE2LE32 (aiffCOMMChunk.numSampleFrames);

				if (flags & RIFF_PARSE_ONLY_HEADER) {
					return 0;
				}
			} else if (chunk.ckid[0] == 'S' &&
			           chunk.ckid[1] == 'S' &&
			           chunk.ckid[2] == 'N' &&
			           chunk.ckid[3] == 'D') {
				/*
				 * Samplecount should be already set with numSampleFrames in COMM chunk.
				 * However in AAF (AIFCDescriptor::Summary), numSampleFrames is often null,
				 * so we must extract samplecount out of SSND chunk, like we do with wav DATA chunk.
				 */
				uint64_t sampleCount = chunk.cksz / RIFFAudioFile->channels / (RIFFAudioFile->sampleSize / 8);

				if (RIFFAudioFile->sampleCount > 0 && RIFFAudioFile->sampleCount != sampleCount) {
					debug ("Sample count retrieved from COMM chunk (%" PRIu64 ") does not match SSND chunk (%" PRIu64 ")", RIFFAudioFile->sampleCount, sampleCount);
				}

				RIFFAudioFile->sampleCount            = sampleCount;
				RIFFAudioFile->pcm_audio_start_offset = pos + sizeof (struct aiffSSNDChunk);

				if (flags & RIFF_PARSE_AAF_SUMMARY) {
					return 0;
				}
			}
		}

		pos_sz = chunk.cksz + sizeof (chunk);

		if (pos_sz >= SIZE_MAX) {
			error ("Parser position is bigger than RIFF_SIZE limits");
			break;
		}

		pos += pos_sz;
	}

	return 0;
}

static uint32_t
beExtended2leUint32 (const unsigned char numx[10])
{
	/*
	 * https://stackoverflow.com/a/18854415/16400184
	 */

	unsigned char x[10];

	/* be -> le */
	x[0] = numx[9];
	x[1] = numx[8];
	x[2] = numx[7];
	x[3] = numx[6];
	x[4] = numx[5];
	x[5] = numx[4];
	x[6] = numx[3];
	x[7] = numx[2];
	x[8] = numx[1];
	x[9] = numx[0];

	int      exponent = (((x[9] << 8) | x[8]) & 0x7FFF);
	uint64_t mantissa = ((uint64_t)x[7] << 56) | ((uint64_t)x[6] << 48) | ((uint64_t)x[5] << 40) | ((uint64_t)x[4] << 32) |
	                    ((uint64_t)x[3] << 24) | ((uint64_t)x[2] << 16) | ((uint64_t)x[1] << 8) | (uint64_t)x[0];
	unsigned char d[8] = { 0 };
	double        result;

	d[7] = x[9] & 0x80; /* Set sign. */

	if ((exponent == 0x7FFF) || (exponent == 0)) {
		/* Infinite, NaN or denormal */
		if (exponent == 0x7FFF) {
			/* Infinite or NaN */
			d[7] |= 0x7F;
			d[6] = 0xF0;
		} else {
			/* Otherwise it's denormal. It cannot be represented as double. Translate as singed zero. */
			memcpy (&result, d, 8);
			return (uint32_t)result;
		}
	} else {
		/* Normal number. */
		exponent = exponent - 0x3FFF + 0x03FF; /*< exponent for double precision. */

		if (exponent <= -52) { /*< Too small to represent. Translate as (signed) zero. */
			memcpy (&result, d, 8);
			return (uint32_t)result;
		} else if (exponent < 0) {
			/* Denormal, exponent bits are already zero here. */
		} else if (exponent >= 0x7FF) { /*< Too large to represent. Translate as infinite. */
			d[7] |= 0x7F;
			d[6] = 0xF0;
			memset (d, 0x00, 6);
			memcpy (&result, d, 8);
			return (uint32_t)result;
		} else {
			/* Representable number */
			d[7] |= (exponent & 0x7F0) >> 4;
			d[6] |= (unsigned char)((exponent & 0xF) << 4);
		}
	}

	/* Translate mantissa. */

	mantissa >>= 11;

	if (exponent < 0) {
		/* Denormal, further shifting is required here. */
		mantissa >>= (-exponent + 1);
	}

	d[0] = mantissa & 0xFF;
	d[1] = (mantissa >> 8) & 0xFF;
	d[2] = (mantissa >> 16) & 0xFF;
	d[3] = (mantissa >> 24) & 0xFF;
	d[4] = (mantissa >> 32) & 0xFF;
	d[5] = (mantissa >> 40) & 0xFF;
	d[6] |= (mantissa >> 48) & 0x0F;

	memcpy (&result, d, 8);

	return (uint32_t)result;
}
