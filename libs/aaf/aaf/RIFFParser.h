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

#ifndef __RIFFParser__
#define __RIFFParser__

#include "aaf/log.h"

#if defined(__linux__)
#include <limits.h>
#include <linux/limits.h>
#elif defined(__APPLE__)
#include <sys/syslimits.h>
#elif defined(_WIN32)
#include <windows.h> // MAX_PATH
#include <limits.h>
#endif

#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__ ((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__) __pragma (pack (push, 1)) __Declaration__ __pragma (pack (pop))
#endif

#define RIFF_READER_ERROR SIZE_MAX

enum RIFF_PARSER_FLAGS {
	RIFF_PARSE_ONLY_HEADER = (1 << 0),
	RIFF_PARSE_AAF_SUMMARY = (1 << 1),
};

struct RIFFAudioFile {
	/* common to wave/aiff */
	uint32_t sampleRate;
	uint16_t sampleSize;
	uint16_t channels;
	uint64_t sampleCount; /* total samples for 1 channel (no matter channel count). (sampleCount / sampleRate) = duration in seconds */
	size_t   pcm_audio_start_offset;
};

PACK (struct riffHeaderChunk {
	char     ckid[4];
	uint32_t cksz;

	char          format[4];
	unsigned char data[];
});

PACK (struct riffChunk {
	char     ckid[4];
	uint32_t cksz;

	unsigned char data[];
});

PACK (struct wavFmtChunk {
	char     ckid[4]; /* 'fmt ' */
	uint32_t cksz;

	uint16_t format_tag;
	uint16_t channels;
	uint32_t samples_per_sec;
	uint32_t avg_bytes_per_sec;
	uint16_t block_align;
	uint16_t bits_per_sample;
});

PACK (struct wavBextChunk {
	char     ckid[4]; /* 'bext' */
	uint32_t cksz;

	char description[256];

	char originator[32];
	char originator_reference[32];
	char origination_date[10];
	char origination_time[8];

	uint64_t time_reference;

	uint16_t version;

	/* 	since bext v1 (2001) 	*/
	unsigned char umid[64];

	/* since bext v2 (2011)
   *
	 * If any loudness parameter is not
	 * being used,  its  value shall be
	 * set to 0x7fff. Any value outside
	 * valid  ranges  shall be ignored.
	 */
	int16_t loudness_value;          // 0xd8f1 - 0xffff (-99.99 -0.1) and 0x000 0x270f (0.00 99.99)
	int16_t loudness_range;          // 0x0000 - 0x270f (0.00  99.99)
	int16_t max_true_peak_level;     // 0xd8f1 - 0xffff (-99.99 -0.1) and 0x000 0x270f (0.00 99.99)
	int16_t max_momentary_loudness;  // 0xd8f1 - 0xffff (-99.99 -0.1) and 0x000 0x270f (0.00 99.99)
	int16_t max_short_term_loudness; // 0xd8f1 - 0xffff (-99.99 -0.1) and 0x000 0x270f (0.00 99.99)

	char reserved[180];

	/*
		Because it is variable size, we
		do not  include  coding history
		in the bext structure. However,
		we know  it  starts at  the end
		of bext structure when parsing.
	*/
});

PACK (struct aiffCOMMChunk {
	char     ckid[4]; /* 'COMM' */
	uint32_t cksz;

	uint16_t      numChannels;
	uint32_t      numSampleFrames;
	uint16_t      sampleSize;
	unsigned char sampleRate[10]; // 80 bit IEEE Standard 754 floating point number
});

PACK (struct aiffSSNDChunk {
	char     ckid[4]; /* 'SSND' */
	uint32_t cksz;

	uint32_t offset;
	uint32_t blockSize;
});

int
laaf_riff_parseAudioFile (struct RIFFAudioFile* RIFFAudioFile, enum RIFF_PARSER_FLAGS flags, size_t (*readerCallback) (unsigned char*, size_t, size_t, void*, void*, void*), void* user1, void* user2, void* user3, struct aafLog* log);

int
laaf_riff_writeWavFileHeader (FILE* fp, struct wavFmtChunk* wavFmt, struct wavBextChunk* wavBext, uint32_t audioDataSize, struct aafLog* log);

#endif // ! __RIFFParser__
