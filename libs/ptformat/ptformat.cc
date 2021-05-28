/*
 * libptformat - a library to read ProTools sessions
 *
 * Copyright (C) 2015-2019  Damien Zammit
 * Copyright (C) 2015-2019  Robin Gareus
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <assert.h>

#ifdef HAVE_GLIB
# include <glib/gstdio.h>
# define ptf_open	g_fopen
#else
# define ptf_open	fopen
#endif

#include "ptformat/ptformat.h"

#define BITCODE			"0010111100101011"
#define ZMARK			'\x5a'
#define ZERO_TICKS		0xe8d4a51000ULL
#define MAX_CONTENT_TYPE	0x3000
#define MAX_CHANNELS_PER_TRACK	8

#if 0
#define DEBUG
#endif

#ifdef DEBUG
#define verbose_printf(...) printf("XXX PTFORMAT XXX: " __VA_ARGS__)
#else
#define verbose_printf(...)
#endif

using namespace std;

static void
hexdump(uint8_t *data, int length, int level)
{
	int i,j,k,end,step=16;

	for (i = 0; i < length; i += step) {
		end = i + step;
		if (end > length) end = length;
		for (k = 0; k < level; k++)
			printf("    ");
		for (j = i; j < end; j++) {
			printf("%02X ", data[j]);
		}
		for (j = i; j < end; j++) {
			if (data[j] < 128 && data[j] > 32)
				printf("%c", data[j]);
			else
				printf(".");
		}
		printf("\n");
	}
}

PTFFormat::PTFFormat()
	: _ptfunxored(0)
	, _len(0)
	, _sessionrate(0)
	, _version(0)
	, _product(NULL)
	, _targetrate (0)
	, _ratefactor (1.0)
	, is_bigendian(false)
{
}

PTFFormat::~PTFFormat() {
	cleanup();
}

const std::string
PTFFormat::get_content_description(uint16_t ctype) {
	switch(ctype) {
	case 0x0030:
		return std::string("INFO product and version");
	case 0x1001:
		return std::string("WAV samplerate, size");
	case 0x1003:
		return std::string("WAV metadata");
	case 0x1004:
		return std::string("WAV list full");
	case 0x1007:
		return std::string("region name, number");
	case 0x1008:
		return std::string("AUDIO region name, number (v5)");
	case 0x100b:
		return std::string("AUDIO region list (v5)");
	case 0x100f:
		return std::string("AUDIO region->track entry");
	case 0x1011:
		return std::string("AUDIO region->track map entries");
	case 0x1012:
		return std::string("AUDIO region->track full map");
	case 0x1014:
		return std::string("AUDIO track name, number");
	case 0x1015:
		return std::string("AUDIO tracks");
	case 0x1017:
		return std::string("PLUGIN entry");
	case 0x1018:
		return std::string("PLUGIN full list");
	case 0x1021:
		return std::string("I/O channel entry");
	case 0x1022:
		return std::string("I/O channel list");
	case 0x1028:
		return std::string("INFO sample rate");
	case 0x103a:
		return std::string("WAV names");
	case 0x104f:
		return std::string("AUDIO region->track subentry (v8)");
	case 0x1050:
		return std::string("AUDIO region->track entry (v8)");
	case 0x1052:
		return std::string("AUDIO region->track map entries (v8)");
	case 0x1054:
		return std::string("AUDIO region->track full map (v8)");
	case 0x1056:
		return std::string("MIDI region->track entry");
	case 0x1057:
		return std::string("MIDI region->track map entries");
	case 0x1058:
		return std::string("MIDI region->track full map");
	case 0x2000:
		return std::string("MIDI events block");
	case 0x2001:
		return std::string("MIDI region name, number (v5)");
	case 0x2002:
		return std::string("MIDI regions map (v5)");
	case 0x2067:
		return std::string("INFO path of session");
	case 0x2511:
		return std::string("Snaps block");
	case 0x2519:
		return std::string("MIDI track full list");
	case 0x251a:
		return std::string("MIDI track name, number");
	case 0x2523:
		return std::string("COMPOUND region element");
	case 0x2602:
		return std::string("I/O route");
	case 0x2603:
		return std::string("I/O routing table");
	case 0x2628:
		return std::string("COMPOUND region group");
	case 0x2629:
		return std::string("AUDIO region name, number (v10)");
	case 0x262a:
		return std::string("AUDIO region list (v10)");
	case 0x262c:
		return std::string("COMPOUND region full map");
	case 0x2633:
		return std::string("MIDI regions name, number (v10)");
	case 0x2634:
		return std::string("MIDI regions map (v10)");
	case 0x271a:
		return std::string("MARKER list");
	default:
		return std::string("UNKNOWN content type");
	}
}

static uint16_t
u_endian_read2(unsigned char *buf, bool bigendian)
{
	if (bigendian) {
		return ((uint16_t)(buf[0]) << 8) | (uint16_t)(buf[1]);
	} else {
		return ((uint16_t)(buf[1]) << 8) | (uint16_t)(buf[0]);
	}
}

static uint32_t
u_endian_read3(unsigned char *buf, bool bigendian)
{
	if (bigendian) {
		return ((uint32_t)(buf[0]) << 16) |
			((uint32_t)(buf[1]) << 8) |
			(uint32_t)(buf[2]);
	} else {
		return ((uint32_t)(buf[2]) << 16) |
			((uint32_t)(buf[1]) << 8) |
			(uint32_t)(buf[0]);
	}
}

static uint32_t
u_endian_read4(unsigned char *buf, bool bigendian)
{
	if (bigendian) {
		return ((uint32_t)(buf[0]) << 24) |
			((uint32_t)(buf[1]) << 16) |
			((uint32_t)(buf[2]) << 8) |
			(uint32_t)(buf[3]);
	} else {
		return ((uint32_t)(buf[3]) << 24) |
			((uint32_t)(buf[2]) << 16) |
			((uint32_t)(buf[1]) << 8) |
			(uint32_t)(buf[0]);
	}
}

static uint64_t
u_endian_read5(unsigned char *buf, bool bigendian)
{
	if (bigendian) {
		return ((uint64_t)(buf[0]) << 32) |
			((uint64_t)(buf[1]) << 24) |
			((uint64_t)(buf[2]) << 16) |
			((uint64_t)(buf[3]) << 8) |
			(uint64_t)(buf[4]);
	} else {
		return ((uint64_t)(buf[4]) << 32) |
			((uint64_t)(buf[3]) << 24) |
			((uint64_t)(buf[2]) << 16) |
			((uint64_t)(buf[1]) << 8) |
			(uint64_t)(buf[0]);
	}
}

static uint64_t
u_endian_read8(unsigned char *buf, bool bigendian)
{
	if (bigendian) {
		return ((uint64_t)(buf[0]) << 56) |
			((uint64_t)(buf[1]) << 48) |
			((uint64_t)(buf[2]) << 40) |
			((uint64_t)(buf[3]) << 32) |
			((uint64_t)(buf[4]) << 24) |
			((uint64_t)(buf[5]) << 16) |
			((uint64_t)(buf[6]) << 8) |
			(uint64_t)(buf[7]);
	} else {
		return ((uint64_t)(buf[7]) << 56) |
			((uint64_t)(buf[6]) << 48) |
			((uint64_t)(buf[5]) << 40) |
			((uint64_t)(buf[4]) << 32) |
			((uint64_t)(buf[3]) << 24) |
			((uint64_t)(buf[2]) << 16) |
			((uint64_t)(buf[1]) << 8) |
			(uint64_t)(buf[0]);
	}
}

void
PTFFormat::cleanup(void) {
	_len = 0;
	_sessionrate = 0;
	_version = 0;
	free(_ptfunxored);
	_ptfunxored = NULL;
	free (_product);
	_product = NULL;
	_audiofiles.clear();
	_regions.clear();
	_midiregions.clear();
	_tracks.clear();
	_miditracks.clear();
	free_all_blocks();
}

int64_t
PTFFormat::foundat(unsigned char *haystack, uint64_t n, const char *needle) {
	int64_t found = 0;
	uint64_t i, j, needle_n;
	needle_n = strlen(needle);

	for (i = 0; i < n; i++) {
		found = i;
		for (j = 0; j < needle_n; j++) {
			if (haystack[i+j] != needle[j]) {
				found = -1;
				break;
			}
		}
		if (found > 0)
			return found;
	}
	return -1;
}

bool
PTFFormat::jumpto(uint32_t *currpos, unsigned char *buf, const uint32_t maxoffset, const unsigned char *needle, const uint32_t needlelen) {
	uint64_t i;
	uint64_t k = *currpos;
	while (k + needlelen < maxoffset) {
		bool foundall = true;
		for (i = 0; i < needlelen; i++) {
			if (buf[k+i] != needle[i]) {
				foundall = false;
				break;
			}
		}
		if (foundall) {
			*currpos = k;
			return true;
		}
		k++;
	}
	return false;
}

bool
PTFFormat::jumpback(uint32_t *currpos, unsigned char *buf, const uint32_t maxoffset, const unsigned char *needle, const uint32_t needlelen) {
	uint64_t i;
	uint64_t k = *currpos;
	while (k > 0 && k + needlelen < maxoffset) {
		bool foundall = true;
		for (i = 0; i < needlelen; i++) {
			if (buf[k+i] != needle[i]) {
				foundall = false;
				break;
			}
		}
		if (foundall) {
			*currpos = k;
			return true;
		}
		k--;
	}
	return false;
}

bool
PTFFormat::foundin(std::string const& haystack, std::string const& needle) {
	size_t found = haystack.find(needle);
	if (found != std::string::npos) {
		return true;
	} else {
		return false;
	}
}

/* Return values:	0            success
			-1           error decrypting pt session
*/
int
PTFFormat::unxor(std::string const& path) {
	FILE *fp;
	unsigned char xxor[256];
	unsigned char ct;
	uint64_t i;
	uint8_t xor_type;
	uint8_t xor_value;
	uint8_t xor_delta;
	uint16_t xor_len;

	if (! (fp = ptf_open(path.c_str(), "rb"))) {
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	_len = ftell(fp);
	if (_len < 0x14) {
		fclose(fp);
		return -1;
	}

	if (! (_ptfunxored = (unsigned char*) malloc(_len * sizeof(unsigned char)))) {
		/* Silently fail -- out of memory*/
		fclose(fp);
		_ptfunxored = 0;
		return -1;
	}

	/* The first 20 bytes are always unencrypted */
	fseek(fp, 0x00, SEEK_SET);
	i = fread(_ptfunxored, 1, 0x14, fp);
	if (i < 0x14) {
		fclose(fp);
		return -1;
	}

	xor_type = _ptfunxored[0x12];
	xor_value = _ptfunxored[0x13];
	xor_len = 256;

	// xor_type 0x01 = ProTools 5, 6, 7, 8 and 9
	// xor_type 0x05 = ProTools 10, 11, 12
	switch(xor_type) {
	case 0x01:
		xor_delta = gen_xor_delta(xor_value, 53, false);
		break;
	case 0x05:
		xor_delta = gen_xor_delta(xor_value, 11, true);
		break;
	default:
		fclose(fp);
		return -1;
	}

	/* Generate the xor_key */
	for (i=0; i < xor_len; i++)
		xxor[i] = (i * xor_delta) & 0xff;

	/* hexdump(xxor, xor_len); */

	/* Read file and decrypt rest of file */
	i = 0x14;
	fseek(fp, i, SEEK_SET);
	while (fread(&ct, 1, 1, fp) != 0) {
		uint8_t xor_index = (xor_type == 0x01) ? i & 0xff : (i >> 12) & 0xff;
		_ptfunxored[i++] = ct ^ xxor[xor_index];
	}
	fclose(fp);
	return 0;
}

/* Return values:	0            success
			-1           error decrypting pt session
			-2           error detecting pt session
			-3           incompatible pt version
			-4           error parsing pt session
*/
int
PTFFormat::load(std::string const& ptf, int64_t targetsr) {
	cleanup();
	_path = ptf;

	if (unxor(_path))
		return -1;

	if (parse_version())
		return -2;

	if (_version < 5 || _version > 12)
		return -3;

	_targetrate = targetsr;

	int err = 0;
	if ((err = parse())) {
		printf ("PARSE FAILED %d\n", err);
		return -4;
	}

	return 0;
}

bool
PTFFormat::parse_version() {
	bool failed = true;
	struct block_t b;

	if (_ptfunxored[0] != '\x03' && foundat(_ptfunxored, 0x100, BITCODE) != 1) {
		return failed;
	}

	is_bigendian = !!_ptfunxored[0x11];

	if (!parse_block_at(0x1f, &b, NULL, 0)) {
		_version = _ptfunxored[0x40];
		if (_version == 0) {
			_version = _ptfunxored[0x3d];
		}
		if (_version == 0) {
			_version = _ptfunxored[0x3a] + 2;
		}
		if (_version != 0)
			failed = false;
		return failed;
	} else {
		if (b.content_type == 0x0003) {
			// old
			uint16_t skip = parsestring(b.offset + 3).size() + 8;
			_version = u_endian_read4(&_ptfunxored[b.offset + 3 + skip], is_bigendian);
			failed = false;
		} else if (b.content_type == 0x2067) {
			// new
			_version = 2 + u_endian_read4(&_ptfunxored[b.offset + 20], is_bigendian);
			failed = false;
		}
		return failed;
	}
}

uint8_t
PTFFormat::gen_xor_delta(uint8_t xor_value, uint8_t mul, bool negative) {
	uint16_t i;
	for (i = 0; i < 256; i++) {
		if (((i * mul) & 0xff) == xor_value) {
				return (negative) ? i * (-1) : i;
		}
	}
	// Should not occur
	return 0;
}

void
PTFFormat::setrates(void) {
	_ratefactor = 1.f;
	if (_sessionrate != 0) {
		_ratefactor = (float)_targetrate / _sessionrate;
	}
}

bool
PTFFormat::parse_block_at(uint32_t pos, struct block_t *block, struct block_t *parent, int level) {
	struct block_t b;
	int childjump = 0;
	uint32_t i;
	uint32_t max = _len;

	if (_ptfunxored[pos] != ZMARK)
		return false;

	if (parent)
		max = parent->block_size + parent->offset;

	b.zmark = ZMARK;
	b.block_type = u_endian_read2(&_ptfunxored[pos+1], is_bigendian);
	b.block_size = u_endian_read4(&_ptfunxored[pos+3], is_bigendian);
	b.content_type = u_endian_read2(&_ptfunxored[pos+7], is_bigendian);
	b.offset = pos + 7;

	if (b.block_size + b.offset > max)
		return false;
	if (b.block_type & 0xff00)
		return false;

	block->zmark = b.zmark;
	block->block_type = b.block_type;
	block->block_size = b.block_size;
	block->content_type = b.content_type;
	block->offset = b.offset;
	block->child.clear();

	for (i = 1; (i < block->block_size) && (pos + i + childjump < max); i += childjump ? childjump : 1) {
		int p = pos + i;
		struct block_t bchild;
		childjump = 0;
		if (parse_block_at(p, &bchild, block, level+1)) {
			block->child.push_back(bchild);
			childjump = bchild.block_size + 7;
		}
	}
	return true;
}

void
PTFFormat::dump_block(struct block_t& b, int level)
{
	int i;

	for (i = 0; i < level; i++) {
		printf("    ");
	}
	printf("%s(0x%04x)\n", get_content_description(b.content_type).c_str(), b.content_type);
	hexdump(&_ptfunxored[b.offset], b.block_size, level);

	for (vector<PTFFormat::block_t>::iterator c = b.child.begin();
			c != b.child.end(); ++c) {
		dump_block(*c, level + 1);
	}
}

void
PTFFormat::free_block(struct block_t& b)
{
	for (vector<PTFFormat::block_t>::iterator c = b.child.begin();
			c != b.child.end(); ++c) {
		free_block(*c);
	}

	b.child.clear();
}

void
PTFFormat::free_all_blocks(void)
{
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		free_block(*b);
	}

	blocks.clear();
}

void
PTFFormat::dump(void) {
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		dump_block(*b, 0);
	}
}

void
PTFFormat::parseblocks(void) {
	uint32_t i = 20;

	while (i < _len) {
		struct block_t b;
		if (parse_block_at(i, &b, NULL, 0)) {
			blocks.push_back(b);
		}
		i += b.block_size ? b.block_size + 7 : 1;
	}
}

int
PTFFormat::parse(void) {
	parseblocks();
#ifdef DEBUG
	dump();
#endif
	if (!parseheader())
		return -1;
	setrates();
	if (_sessionrate < 44100 || _sessionrate > 192000)
		return -2;
	if (!parseaudio())
		return -3;
	if (!parserest())
		return -4;
	if (!parsemidi())
		return -5;
	return 0;
}

bool
PTFFormat::parseheader(void) {
	bool found = false;

	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		if (b->content_type == 0x1028) {
			_sessionrate = u_endian_read4(&_ptfunxored[b->offset+4], is_bigendian);
			found = true;
		}
	}
	return found;
}

std::string
PTFFormat::parsestring (uint32_t pos) {
	uint32_t length = u_endian_read4(&_ptfunxored[pos], is_bigendian);
	pos += 4;
	return std::string((const char *)&_ptfunxored[pos], length);
}

bool
PTFFormat::parseaudio(void) {
	bool found = false;
	uint32_t nwavs = 0;
	uint32_t i, n;
	uint32_t pos = 0;
	std::string wavtype;
	std::string wavname;

	// Parse wav names
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		if (b->content_type == 0x1004) {

			nwavs = u_endian_read4(&_ptfunxored[b->offset+2], is_bigendian);

			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if (c->content_type == 0x103a) {
					//nstrings = u_endian_read4(&_ptfunxored[c->offset+1], is_bigendian);
					pos = c->offset + 11;
					// Found wav list
					for (i = n = 0; (pos < c->offset + c->block_size) && (n < nwavs); i++) {
						wavname = parsestring(pos);
						pos += wavname.size() + 4;
						wavtype = std::string((const char*)&_ptfunxored[pos], 4);
						pos += 9;
						if (foundin(wavname, std::string(".grp")))
							continue;

						if (foundin(wavname, std::string("Audio Files"))) {
							continue;
						}
						if (foundin(wavname, std::string("Fade Files"))) {
							continue;
						}
						if (_version < 10) {
							if (!(foundin(wavtype, std::string("WAVE")) ||
									foundin(wavtype, std::string("EVAW")) ||
									foundin(wavtype, std::string("AIFF")) ||
									foundin(wavtype, std::string("FFIA"))) ) {
								continue;
							}
						} else {
							if (wavtype[0] != '\0') {
								if (!(foundin(wavtype, std::string("WAVE")) ||
										foundin(wavtype, std::string("EVAW")) ||
										foundin(wavtype, std::string("AIFF")) ||
										foundin(wavtype, std::string("FFIA"))) ) {
									continue;
								}
							} else if (!(foundin(wavname, std::string(".wav")) || 
									foundin(wavname, std::string(".aif"))) ) {
								continue;
							}
						}
						found = true;
						wav_t f (n);
						f.filename = wavname;
						n++;
						_audiofiles.push_back(f);
					}
				}
			}
		}
	}

	if (!found) {
		if (nwavs > 0) {
			return false;
		} else {
			return true;
		}
	}

	// Add wav length information
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		if (b->content_type == 0x1004) {

			vector<PTFFormat::wav_t>::iterator wav = _audiofiles.begin();

			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if (c->content_type == 0x1003) {
					for (vector<PTFFormat::block_t>::iterator d = c->child.begin();
							d != c->child.end(); ++d) {
						if (d->content_type == 0x1001) {
							(*wav).length = u_endian_read8(&_ptfunxored[d->offset+8], is_bigendian);
							wav++;
						}
					}
				}
			}
		}
	}

	return found;
}


void
PTFFormat::parse_three_point(uint32_t j, uint64_t& start, uint64_t& offset, uint64_t& length) {
	uint8_t offsetbytes, lengthbytes, startbytes;

	if (is_bigendian) {
		offsetbytes = (_ptfunxored[j+4] & 0xf0) >> 4;
		lengthbytes = (_ptfunxored[j+3] & 0xf0) >> 4;
		startbytes = (_ptfunxored[j+2] & 0xf0) >> 4;
		//somethingbytes = (_ptfunxored[j+2] & 0xf);
		//skipbytes = _ptfunxored[j+1];
	} else {
		offsetbytes = (_ptfunxored[j+1] & 0xf0) >> 4; //3
		lengthbytes = (_ptfunxored[j+2] & 0xf0) >> 4;
		startbytes = (_ptfunxored[j+3] & 0xf0) >> 4; //1
		//somethingbytes = (_ptfunxored[j+3] & 0xf);
		//skipbytes = _ptfunxored[j+4];
	}

	switch (offsetbytes) {
	case 5:
		offset = u_endian_read5(&_ptfunxored[j+5], false);
		break;
	case 4:
		offset = (uint64_t)u_endian_read4(&_ptfunxored[j+5], false);
		break;
	case 3:
		offset = (uint64_t)u_endian_read3(&_ptfunxored[j+5], false);
		break;
	case 2:
		offset = (uint64_t)u_endian_read2(&_ptfunxored[j+5], false);
		break;
	case 1:
		offset = (uint64_t)(_ptfunxored[j+5]);
		break;
	default:
		offset = 0;
		break;
	}
	j+=offsetbytes;
	switch (lengthbytes) {
	case 5:
		length = u_endian_read5(&_ptfunxored[j+5], false);
		break;
	case 4:
		length = (uint64_t)u_endian_read4(&_ptfunxored[j+5], false);
		break;
	case 3:
		length = (uint64_t)u_endian_read3(&_ptfunxored[j+5], false);
		break;
	case 2:
		length = (uint64_t)u_endian_read2(&_ptfunxored[j+5], false);
		break;
	case 1:
		length = (uint64_t)(_ptfunxored[j+5]);
		break;
	default:
		length = 0;
		break;
	}
	j+=lengthbytes;
	switch (startbytes) {
	case 5:
		start = u_endian_read5(&_ptfunxored[j+5], false);
		break;
	case 4:
		start = (uint64_t)u_endian_read4(&_ptfunxored[j+5], false);
		break;
	case 3:
		start = (uint64_t)u_endian_read3(&_ptfunxored[j+5], false);
		break;
	case 2:
		start = (uint64_t)u_endian_read2(&_ptfunxored[j+5], false);
		break;
	case 1:
		start = (uint64_t)(_ptfunxored[j+5]);
		break;
	default:
		start = 0;
		break;
	}
}

void
PTFFormat::parse_region_info(uint32_t j, block_t& blk, region_t& r) {
	uint64_t findex, start, sampleoffset, length;

	parse_three_point(j, start, sampleoffset, length);

	findex = u_endian_read4(&_ptfunxored[blk.offset + blk.block_size], is_bigendian);
	wav_t f (findex);
	f.posabsolute = start * _ratefactor;
	f.length = length * _ratefactor;

	wav_t found;
	if (find_wav(findex, found)) {
		f.filename = found.filename;
	}

	std::vector<midi_ev_t> m;
	r.startpos = (int64_t)(start*_ratefactor);
	r.sampleoffset = (int64_t)(sampleoffset*_ratefactor);
	r.length = (int64_t)(length*_ratefactor);
	r.wave = f;
	r.midi = m;
}

bool
PTFFormat::parserest(void) {
	uint32_t i, j, count;
	uint64_t start;
	uint16_t rindex, rawindex, tindex, mindex;
	uint32_t nch;
	uint16_t ch_map[MAX_CHANNELS_PER_TRACK];
	bool found = false;
	bool region_is_fade = false;
	std::string regionname, trackname, midiregionname;
	rindex = 0;

	// Parse sources->regions
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		if (b->content_type == 0x100b || b->content_type == 0x262a) {
			//nregions = u_endian_read4(&_ptfunxored[b->offset+2], is_bigendian);
			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if (c->content_type == 0x1008 || c->content_type == 0x2629) {
					vector<PTFFormat::block_t>::iterator d = c->child.begin();
					region_t r;

					found = true;
					j = c->offset + 11;
					regionname = parsestring(j);
					j += regionname.size() + 4;

					r.name = regionname;
					r.index = rindex;
					parse_region_info(j, *d, r);

					_regions.push_back(r);
					rindex++;
				}
			}
			found = true;
		}
	}

	// Parse tracks
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		if (b->content_type == 0x1015) {
			//ntracks = u_endian_read4(&_ptfunxored[b->offset+2], is_bigendian);
			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if (c->content_type == 0x1014) {
					j = c->offset + 2;
					trackname = parsestring(j);
					j += trackname.size() + 5;
					nch = u_endian_read4(&_ptfunxored[j], is_bigendian);
					j += 4;
					for (i = 0; i < nch; i++) {
						ch_map[i] = u_endian_read2(&_ptfunxored[j], is_bigendian);

						track_t ti;
						if (!find_track(ch_map[i], ti)) {
							// Add a dummy region for now
							region_t r (65535);
							track_t t (ch_map[i]);
							t.name = trackname;
							t.reg = r;
							_tracks.push_back(t);
						}
						//verbose_printf("%s : %d(%d)\n", reg, nch, ch_map[0]);
						j += 2;
					}
				}
			}
		}
	}

	// Reparse from scratch to exclude audio tracks from all tracks to get midi tracks
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		if (b->content_type == 0x2519) {
			tindex = 0;
			mindex = 0;
			//ntracks = u_endian_read4(&_ptfunxored[b->offset+2], is_bigendian);
			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if (c->content_type == 0x251a) {
					j = c->offset + 4;
					trackname = parsestring(j);
					j += trackname.size() + 4 + 18;
					//tindex = u_endian_read4(&_ptfunxored[j], is_bigendian);

					// Add a dummy region for now
					region_t r (65535);
					track_t t (mindex);
					t.name = trackname;
					t.reg = r;

					track_t ti;
					// If the current track is not an audio track, insert as midi track
					if (!(find_track(tindex, ti) && foundin(trackname, ti.name))) {
						_miditracks.push_back(t);
						mindex++;
					}
					tindex++;
				}
			}
		}
	}

	// Parse regions->tracks
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		tindex = 0;
		if (b->content_type == 0x1012) {
			//nregions = u_endian_read4(&_ptfunxored[b->offset+2], is_bigendian);
			count = 0;
			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if (c->content_type == 0x1011) {
					regionname = parsestring(c->offset + 2);
					for (vector<PTFFormat::block_t>::iterator d = c->child.begin();
							d != c->child.end(); ++d) {
						if (d->content_type == 0x100f) {
							for (vector<PTFFormat::block_t>::iterator e = d->child.begin();
									e != d->child.end(); ++e) {
								if (e->content_type == 0x100e) {
									// Region->track
									track_t ti;
									j = e->offset + 4;
									rawindex = u_endian_read4(&_ptfunxored[j], is_bigendian);
									if (!find_track(count, ti))
										continue;
									if (!find_region(rawindex, ti.reg))
										continue;
									if (ti.reg.index != 65535) {
										_tracks.push_back(ti);
									}
								}
							}
						}
					}
					found = true;
					count++;
				}
			}
		} else if (b->content_type == 0x1054) {
			//nregions = u_endian_read4(&_ptfunxored[b->offset+2], is_bigendian);
			count = 0;
			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if (c->content_type == 0x1052) {
					trackname = parsestring(c->offset + 2);
					for (vector<PTFFormat::block_t>::iterator d = c->child.begin();
							d != c->child.end(); ++d) {
						if (d->content_type == 0x1050) {
							region_is_fade = (_ptfunxored[d->offset + 46] == 0x01);
							if (region_is_fade) {
								verbose_printf("dropped fade region\n");
								continue;
							}
							for (vector<PTFFormat::block_t>::iterator e = d->child.begin();
									e != d->child.end(); ++e) {
								if (e->content_type == 0x104f) {
									// Region->track
									j = e->offset + 4;
									rawindex = u_endian_read4(&_ptfunxored[j], is_bigendian);
									j += 4 + 1;
									start = u_endian_read4(&_ptfunxored[j], is_bigendian);
									tindex = count;
									track_t ti;
									if (!find_track(tindex, ti)) {
										verbose_printf("dropped track %d\n", tindex);
										continue;
									}
									if (!find_region(rawindex, ti.reg)) {
										verbose_printf("dropped region %d\n", rawindex);
										continue;
									}
									ti.reg.startpos = start * _ratefactor;
									if (ti.reg.index != 65535) {
										_tracks.push_back(ti);
									}
								}
							}
						}
					}
					found = true;
					count++;
				}
			}
		}
	}
	for (std::vector<track_t>::iterator tr = _tracks.begin();
			tr != _tracks.end(); /* noop */) {
		if ((*tr).reg.index == 65535) {
			tr = _tracks.erase(tr);
		} else {
			tr++;
		}
	}
	return found;
}

struct mchunk {
	mchunk (uint64_t zt, uint64_t ml, std::vector<PTFFormat::midi_ev_t> const& c)
	: zero (zt)
	, maxlen (ml)
	, chunk (c)
	{}
	uint64_t zero;
	uint64_t maxlen;
	std::vector<PTFFormat::midi_ev_t> chunk;
};

bool
PTFFormat::parsemidi(void) {
	uint32_t i, j, k, n, rindex, tindex, mindex, count, rawindex;
	uint64_t n_midi_events, zero_ticks, start, offset, length, start2, stop2;
	uint64_t midi_pos, midi_len, max_pos, region_pos;
	uint8_t midi_velocity, midi_note;
	uint16_t regionnumber = 0;
	std::string midiregionname;

	std::vector<mchunk> midichunks;
	midi_ev_t m;

	std::string regionname, trackname;
	rindex = 0;

	// Parse MIDI events
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		if (b->content_type == 0x2000) {

			k = b->offset;

			// Parse all midi chunks, not 1:1 mapping to regions yet
			while (k + 35 < b->block_size + b->offset) {
				max_pos = 0;
				std::vector<midi_ev_t> midi;

				if (!jumpto(&k, _ptfunxored, _len, (const unsigned char *)"MdNLB", 5)) {
					break;
				}
				k += 11;
				n_midi_events = u_endian_read4(&_ptfunxored[k], is_bigendian);

				k += 4;
				zero_ticks = u_endian_read5(&_ptfunxored[k], is_bigendian);
				for (i = 0; i < n_midi_events && k < _len; i++, k += 35) {
					midi_pos = u_endian_read5(&_ptfunxored[k], is_bigendian);
					midi_pos -= zero_ticks;
					midi_note = _ptfunxored[k+8];
					midi_len = u_endian_read5(&_ptfunxored[k+9], is_bigendian);
					midi_velocity = _ptfunxored[k+17];

					if (midi_pos + midi_len > max_pos) {
						max_pos = midi_pos + midi_len;
					}

					m.pos = midi_pos;
					m.length = midi_len;
					m.note = midi_note;
					m.velocity = midi_velocity;
					midi.push_back(m);
				}
				midichunks.push_back(mchunk (zero_ticks, max_pos, midi));
			}

		// Put chunks onto regions
		} else if ((b->content_type == 0x2002) || (b->content_type == 0x2634)) {
			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if ((c->content_type == 0x2001) || (c->content_type == 0x2633)) {
					for (vector<PTFFormat::block_t>::iterator d = c->child.begin();
							d != c->child.end(); ++d) {
						if ((d->content_type == 0x1007) || (d->content_type == 0x2628)) {
							j = d->offset + 2;
							midiregionname = parsestring(j);
							j += 4 + midiregionname.size();
							parse_three_point(j, region_pos, zero_ticks, midi_len);
							j = d->offset + d->block_size;
							rindex = u_endian_read4(&_ptfunxored[j], is_bigendian);
							struct mchunk mc = *(midichunks.begin()+rindex);

							region_t r (regionnumber++);
							r.name = midiregionname;
							r.startpos = (int64_t)0xe8d4a51000ULL;
							r.sampleoffset = 0;
							r.length = mc.maxlen;
							r.midi = mc.chunk;

							_midiregions.push_back(r);
							//verbose_printf("MIDI %s : r(%d) (%llu, %llu, %llu)\n", str, rindex, zero_ticks, region_pos, midi_len);
							//dump_block(*d, 1);
						}
					}
				}
			}
		} 
	}
	
	// COMPOUND MIDI regions
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		if (b->content_type == 0x262c) {
			mindex = 0;
			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if (c->content_type == 0x262b) {
					for (vector<PTFFormat::block_t>::iterator d = c->child.begin();
							d != c->child.end(); ++d) {
						if (d->content_type == 0x2628) {
							count = 0;
							j = d->offset + 2;
							regionname = parsestring(j);
							j += 4 + regionname.size();
							parse_three_point(j, start, offset, length);
							j = d->offset + d->block_size + 2;
							n = u_endian_read2(&_ptfunxored[j], is_bigendian);

							for (vector<PTFFormat::block_t>::iterator e = d->child.begin();
									e != d->child.end(); ++e) {
								if (e->content_type == 0x2523) {
									// FIXME Compound MIDI region
									j = e->offset + 39;
									rawindex = u_endian_read4(&_ptfunxored[j], is_bigendian);
									j += 12; 
									start2 = u_endian_read5(&_ptfunxored[j], is_bigendian);
									int64_t signedval = (int64_t)start2;
									signedval -= ZERO_TICKS;
									if (signedval < 0) {
										signedval = -signedval;
									}
									start2 = signedval;
									j += 8;
									stop2 = u_endian_read5(&_ptfunxored[j], is_bigendian);
									signedval = (int64_t)stop2;
									signedval -= ZERO_TICKS;
									if (signedval < 0) {
										signedval = -signedval;
									}
									stop2 = signedval;
									j += 16;
									//nn = u_endian_read4(&_ptfunxored[j], is_bigendian);
									//verbose_printf("COMPOUND %s : c(%d) r(%d) ?(%d) ?(%d) (%llu %llu)(%llu %llu %llu)\n", str, mindex, rawindex, n, nn, start2, stop2, start, offset, length);
									count++;
								}
							}
							if (!count) {
								// Plain MIDI region
								struct mchunk mc = *(midichunks.begin()+n);

								region_t r (n);
								r.name = midiregionname;
								r.startpos = (int64_t)0xe8d4a51000ULL;
								r.length = mc.maxlen;
								r.midi = mc.chunk;
								_midiregions.push_back(r);
								verbose_printf("%s : MIDI region mr(%d) ?(%d) (%lu %lu %lu)\n", regionname.c_str(), mindex, n, start, offset, length);
								mindex++;
							}
						}
					}
				}
			}
		} 
	}
	
	// Put midi regions onto midi tracks
	for (vector<PTFFormat::block_t>::iterator b = blocks.begin();
			b != blocks.end(); ++b) {
		if (b->content_type == 0x1058) {
			//nregions = u_endian_read4(&_ptfunxored[b->offset+2], is_bigendian);
			count = 0;
			for (vector<PTFFormat::block_t>::iterator c = b->child.begin();
					c != b->child.end(); ++c) {
				if (c->content_type == 0x1057) {
					regionname = parsestring(c->offset + 2);
					for (vector<PTFFormat::block_t>::iterator d = c->child.begin();
							d != c->child.end(); ++d) {
						if (d->content_type == 0x1056) {
							for (vector<PTFFormat::block_t>::iterator e = d->child.begin();
									e != d->child.end(); ++e) {
								if (e->content_type == 0x104f) {
									// MIDI region->MIDI track
									track_t ti;
									j = e->offset + 4;
									rawindex = u_endian_read4(&_ptfunxored[j], is_bigendian);
									j += 4 + 1;
									start = u_endian_read5(&_ptfunxored[j], is_bigendian);
									tindex = count;
									if (!find_miditrack(tindex, ti)) {
										verbose_printf("dropped midi t(%d) r(%d)\n", tindex, rawindex);
										continue;
									}
									if (!find_midiregion(rawindex, ti.reg)) {
										verbose_printf("dropped midiregion\n");
										continue;
									}
									//verbose_printf("MIDI : %s : t(%d) r(%d) %llu(%llu)\n", ti.name.c_str(), tindex, rawindex, start, ti.reg.startpos);
									int64_t signedstart = (int64_t)(start - ZERO_TICKS);
									if (signedstart < 0)
										signedstart = -signedstart;
									ti.reg.startpos = (uint64_t)(signedstart * _ratefactor);
									if (ti.reg.index != 65535) {
										_miditracks.push_back(ti);
									}
								}
							}
						}
					}
					count++;
				}
			}
		}
	}
	for (std::vector<track_t>::iterator tr = _miditracks.begin();
			tr != _miditracks.end(); /* noop */) {
		if ((*tr).reg.index == 65535) {
			tr = _miditracks.erase(tr);
		} else {
			tr++;
		}
	}
	return true;
}
