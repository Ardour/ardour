/*
    Copyright (C) 2015  Damien Zammit
    Copyright (C) 2015  Robin Gareus

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/

#include "ptfformat.h"

#include <stdio.h>
#include <string>
#include <string.h>
#include <assert.h>

using namespace std;

static const uint32_t baselut[16] = {
	0xaaaaaaaa, 0xaa955555, 0xa9554aaa, 0xa552a955,
	0xb56ad5aa, 0x95a95a95, 0x94a5294a, 0x9696b4b5,
	0xd2d25a5a, 0xd24b6d25, 0xdb6db6da, 0xd9249b6d,
	0xc9b64d92, 0xcd93264d, 0xccd99b32, 0xcccccccd
};

static const uint32_t xorlut[16] = {
	0x00000000, 0x00000b00, 0x000100b0, 0x00b0b010,
	0x010b0b01, 0x0b10b10b, 0x01bb101b, 0x0111bbbb,
	0x1111bbbb, 0x1bbb10bb, 0x1bb0bb0b, 0xbb0b0bab,
	0xbab0b0ba, 0xb0abaaba, 0xba0aabaa, 0xbaaaaaaa
};

static uint32_t swapbytes32 (const uint32_t v) {
	uint32_t rv = 0;
	rv |= ((v >>  0) & 0xf) << 28;
	rv |= ((v >>  4) & 0xf) << 24;
	rv |= ((v >>  8) & 0xf) << 20;
	rv |= ((v >> 12) & 0xf) << 16;
	rv |= ((v >> 16) & 0xf) << 12;
	rv |= ((v >> 20) & 0xf) <<  8;
	rv |= ((v >> 24) & 0xf) <<  4;
	rv |= ((v >> 28) & 0xf) <<  0;
	return rv;
}

static uint64_t gen_secret (int i) {
	assert (i > 0 && i < 256);
	int iwrap = i & 0x7f; // wrap at 0x80;
	uint32_t xor_lo = 0;  // 0x40 flag
	int idx;              // mirror at 0x40;

	if (iwrap & 0x40) {
		xor_lo = 0x1;
		idx    = 0x80 - iwrap;
	} else {
		idx    = iwrap;
	}

	int i16 = (idx >> 1) & 0xf;
	if (idx & 0x20) {
		i16 = 15 - i16;
	}

	uint32_t lo = baselut [i16];
	uint32_t xk = xorlut  [i16];

	if (idx & 0x20) {
		lo ^= 0xaaaaaaab;
		xk ^= 0x10000000;
	}
	uint32_t hi = swapbytes32 (lo) ^ xk;
	return  ((uint64_t)hi << 32) | (lo ^ xor_lo);
}

PTFFormat::PTFFormat() {
}

PTFFormat::~PTFFormat() {
	if (ptfunxored) {
		free(ptfunxored);
	}
}

bool
PTFFormat::foundin(std::string haystack, std::string needle) {
	size_t found = haystack.find(needle);
	if (found != std::string::npos) {
		return true;
	} else {
		return false;
	}
}

/* Return values:	0            success
			0x01 to 0xff value of missing lut
			-1           could not open file as ptf
*/
int
PTFFormat::load(std::string path, int64_t targetsr) {
	FILE *fp;
	unsigned char xxor[256];
	unsigned char ct;
	unsigned char v;
	unsigned char voff;
	uint64_t key;
	uint64_t i;
	uint64_t j;
	int inv;
	int err;

	if (! (fp = fopen(path.c_str(), "rb"))) {
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	if (len < 0x40) {
		fclose(fp);
		return -1;
	}
	fseek(fp, 0x40, SEEK_SET);
	fread(&c0, 1, 1, fp);
	fread(&c1, 1, 1, fp);

	// For version <= 7 support:
	version = c0 & 0x0f;
	c0 = c0 & 0xc0;

	if (! (ptfunxored = (unsigned char*) malloc(len * sizeof(unsigned char)))) {
		/* Silently fail -- out of memory*/
		fclose(fp);
		ptfunxored = 0;
		return -1;
	}

	switch (c0) {
	case 0x00:
		// Success! easy one
		xxor[0] = c0;
		xxor[1] = c1;
		//fprintf(stderr, "0 %02x\n1 %02x\n", c0, c1);

		for (i = 2; i < 256; i++) {
			if (i%64 == 0) {
				xxor[i] = c0;
			} else {
				xxor[i] = (xxor[i-1] + c1 - c0) & 0xff;
				//fprintf(stderr, "%x %02x\n", i, xxor[i]);
			}
		}
		break;
	case 0x80:
		//Success! easy two
		xxor[0] = c0;
		xxor[1] = c1;
		for (i = 2; i < 256; i++) {
			if (i%64 == 0) {
				xxor[i] = c0;
			} else {
				xxor[i] = ((xxor[i-1] + c1 - c0) & 0xff);
			}
		}
		for (i = 0; i < 64; i++) {
			xxor[i] ^= 0x80;
		}
		for (i = 128; i < 192; i++) {
			xxor[i] ^= 0x80;
		}
		break;
	case 0x40:
	case 0xc0:
		xxor[0] = c0;
		xxor[1] = c1;
		for (i = 2; i < 256; i++) {
			if (i%64 == 0) {
				xxor[i] = c0;
			} else {
				xxor[i] = ((xxor[i-1] + c1 - c0) & 0xff);
			}
		}

		key = gen_secret(c1);

		for (i = 0; i < 64; i++) {
			xxor[i] ^= (((key >> i) & 1) * 2 * 0x40) + 0x40;
		}
		for (i = 128; i < 192; i++) {
			inv = (((key >> (i-128)) & 1) == 1) ? 1 : 3;
			xxor[i] ^= (inv * 0x40);
		}

		for (i = 192; i < 256; i++) {
			xxor[i] ^= 0x80;
		}
		break;
		break;
	default:
		//Should not happen, failed c[0] c[1]
		return -1;
		break;
	}

	/* Read file */
	i = 0;
	fseek(fp, 0, SEEK_SET);
	while (fread(&ct, 1, 1, fp) != 0) {
		ptfunxored[i++] = ct;
	}
	fclose(fp);

	/* version detection */
	voff = 0x36;
	v = ptfunxored[voff];
	if (v == 0x20) {
		voff += 7;
	} else if (v == 0x03) {
		voff += 4;
	} else {
		voff = 0;
	}
	v = ptfunxored[voff];
	if (v == 10 || v == 11 || v == 12) {
		version = v;
		unxor10();
	}

	if (version == 0 || version == 5 || version == 7) {
		/* Haven't detected version yet so decipher */
		j = 0;
		for (i = 0; i < len; i++) {
			if (j%256 == 0) {
				j = 0;
			}
			ptfunxored[i] ^= xxor[j];
			j++;
		}

		/* version detection */
		voff = 0x36;
		v = ptfunxored[voff];
		if (v == 0x20) {
			voff += 7;
		} else if (v == 0x03) {
			voff += 4;
		} else {
			voff = 0;
		}
		v = ptfunxored[voff];
		if (v == 5 || v == 7 || v == 8 || v == 9) {
			version = v;
		}
	}

	if (version < 5 || version > 12)
		return -1;
	targetrate = targetsr;
	err = parse();
	if (err)
		return -1;
	return 0;
}

uint8_t
PTFFormat::mostfrequent(uint32_t start, uint32_t stop)
{
	uint32_t counts[256] = {0};
	uint64_t i;
	uint32_t max = 0;
	uint8_t maxi = 0;

	for (i = start; i < stop; i++) {
		counts[ptfunxored[i]]++;
	}

	for (i = 0; i < 256; i++) {
		if (counts[i] > max) {
			maxi = i;
			max = counts[i];
		}
	}
	return maxi;
}

void
PTFFormat::unxor10(void)
{
	uint64_t j;
	uint8_t x = mostfrequent(0x1000, 0x2000);
	uint8_t dx = 0x100-x;

	for (j = 0x1000; j < len; j++) {
		if(j % 0x1000 == 0xfff) {
			x = (x - dx) & 0xff;
		}
		ptfunxored[j] ^= x;
	}
}

int
PTFFormat::parse(void) {
	if (version == 5) {
		parse5header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio5();
		parserest5();
	} else if (version == 7) {
		parse7header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio();
		parserest89();
	} else if (version == 8) {
		parse8header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio();
		parserest89();
	} else if (version == 9) {
		parse9header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio();
		parserest89();
	} else if (version == 10 || version == 11 || version == 12) {
		parse10header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio();
		parserest10();
	} else {
		// Should not occur
		return -1;
	}
	return 0;
}

void
PTFFormat::setrates(void) {
	ratefactor = 1.f;
	if (sessionrate != 0) {
		ratefactor = (float)targetrate / sessionrate;
	}
}

void
PTFFormat::parse5header(void) {
	uint32_t k;

	// Find session sample rate
	k = 0x100;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5a) &&
				(ptfunxored[k+1] == 0x00) &&
				(ptfunxored[k+2] == 0x02)) {
			break;
		}
		k++;
	}

	sessionrate = 0;
	sessionrate |= ptfunxored[k+12] << 16;
	sessionrate |= ptfunxored[k+13] << 8;
	sessionrate |= ptfunxored[k+14];
}

void
PTFFormat::parse7header(void) {
	uint64_t k;

	// Find session sample rate
	k = 0x100;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5a) &&
				(ptfunxored[k+1] == 0x00) &&
				(ptfunxored[k+2] == 0x05)) {
			break;
		}
		k++;
	}

	sessionrate = 0;
	sessionrate |= ptfunxored[k+12] << 16;
	sessionrate |= ptfunxored[k+13] << 8;
	sessionrate |= ptfunxored[k+14];
}

void
PTFFormat::parse8header(void) {
	uint64_t k;

	// Find session sample rate
	k = 0;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5a) &&
				(ptfunxored[k+1] == 0x05)) {
			break;
		}
		k++;
	}

	sessionrate = 0;
	sessionrate |= ptfunxored[k+11];
	sessionrate |= ptfunxored[k+12] << 8;
	sessionrate |= ptfunxored[k+13] << 16;
}

void
PTFFormat::parse9header(void) {
	uint64_t k;

	// Find session sample rate
	k = 0x100;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5a) &&
				(ptfunxored[k+1] == 0x06)) {
			break;
		}
		k++;
	}

	sessionrate = 0;
	sessionrate |= ptfunxored[k+11];
	sessionrate |= ptfunxored[k+12] << 8;
	sessionrate |= ptfunxored[k+13] << 16;
}

void
PTFFormat::parse10header(void) {
	uint64_t k;

	// Find session sample rate
	k = 0x100;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5a) &&
				(ptfunxored[k+1] == 0x09)) {
			break;
		}
		k++;
	}

	sessionrate = 0;
	sessionrate |= ptfunxored[k+11];
	sessionrate |= ptfunxored[k+12] << 8;
	sessionrate |= ptfunxored[k+13] << 16;
}

void
PTFFormat::parserest5(void) {
	uint64_t i, j, k;
	uint64_t regionspertrack, lengthofname;
	uint64_t startbytes, lengthbytes, offsetbytes;
	uint16_t tracknumber = 0;
	uint16_t findex;
	uint16_t rindex;

	k = 0;
	for (i = 0; i < 5; i++) {
		while (k < len) {
			if (		(ptfunxored[k  ] == 0x5a) &&
					(ptfunxored[k+1] == 0x00) &&
					(ptfunxored[k+2] == 0x03)) {
				break;
			}
			k++;
		}
		k++;
	}
	k--;

	for (i = 0; i < 2; i++) {
		while (k) {
			if (		(ptfunxored[k  ] == 0x5a) &&
					(ptfunxored[k+1] == 0x00) &&
					(ptfunxored[k+2] == 0x01)) {
				break;
			}
			k--;
		}
		if (k)
			k--;
	}
	k++;

	rindex = 0;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0xff) &&
				(ptfunxored[k+1] == 0xff)) {
			break;
		}
		while (k < len) {
			if (		(ptfunxored[k  ] == 0x5a) &&
					(ptfunxored[k+1] == 0x00) &&
					(ptfunxored[k+2] == 0x01)) {
				break;
			}
			k++;
		}

		lengthofname = ptfunxored[k+12];
		if (ptfunxored[k+13] == 0x5a) {
			k++;
			break;
		}
		char name[256] = {0};
		for (j = 0; j < lengthofname; j++) {
			name[j] = ptfunxored[k+13+j];
		}
		name[j] = '\0';
		regionspertrack = ptfunxored[k+13+j+3];
		for (i = 0; i < regionspertrack; i++) {
			while (k < len) {
				if (		(ptfunxored[k  ] == 0x5a) &&
						(ptfunxored[k+1] == 0x00) &&
						(ptfunxored[k+2] == 0x03)) {
					break;
				}
				k++;
			}
			j = k+16;
			startbytes = (ptfunxored[j+3] & 0xf0) >> 4;
			lengthbytes = (ptfunxored[j+2] & 0xf0) >> 4;
			offsetbytes = (ptfunxored[j+1] & 0xf0) >> 4;
			//somethingbytes = (ptfunxored[j+1] & 0xf);
			findex = ptfunxored[k+14];
			j--;
			uint32_t start = 0;
			switch (startbytes) {
			case 4:
				start |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				start |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				start |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				start |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=startbytes;
			uint32_t length = 0;
			switch (lengthbytes) {
			case 4:
				length |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				length |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				length |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				length |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=lengthbytes;
			uint32_t sampleoffset = 0;
			switch (offsetbytes) {
			case 4:
				sampleoffset |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				sampleoffset |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				sampleoffset |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				sampleoffset |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=offsetbytes;

			//printf("name=`%s` start=%04x length=%04x offset=%04x findex=%d\n", name,start,length,sampleoffset,findex);

			std::string filename = string(name) + extension;
			wav_t f = {
				filename,
				findex,
				(int64_t)(start*ratefactor),
				(int64_t)(length*ratefactor),
			};

			vector<wav_t>::iterator begin = audiofiles.begin();
			vector<wav_t>::iterator finish = audiofiles.end();
			vector<wav_t>::iterator found;
			// Add file to lists
			if ((found = std::find(begin, finish, f)) != finish) {
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					*found,
				};
				regions.push_back(r);
				vector<track_t>::iterator ti;
				vector<track_t>::iterator bt = tracks.begin();
				vector<track_t>::iterator et = tracks.end();
				track_t tr = { name, 0, 0, r };
				if ((ti = std::find(bt, et, tr)) != et) {
					tracknumber = (*ti).index;
				} else {
					tracknumber = tracks.size() + 1;
				}
				track_t t = {
					name,
					(uint16_t)tracknumber,
					uint8_t(0),
					r
				};
				tracks.push_back(t);
			} else {
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f,
				};
				regions.push_back(r);
				vector<track_t>::iterator ti;
				vector<track_t>::iterator bt = tracks.begin();
				vector<track_t>::iterator et = tracks.end();
				track_t tr = { name, 0, 0, r };
				if ((ti = std::find(bt, et, tr)) != et) {
					tracknumber = (*ti).index;
				} else {
					tracknumber = tracks.size() + 1;
				}
				track_t t = {
					name,
					(uint16_t)tracknumber,
					uint8_t(0),
					r
				};
				tracks.push_back(t);
			}
			rindex++;
			k++;
		}
		k++;
	}
}

void
PTFFormat::resort(std::vector<wav_t>& ws) {
	int j = 0;
	std::sort(ws.begin(), ws.end());
	for (std::vector<wav_t>::iterator i = ws.begin(); i != ws.end(); ++i) {
		(*i).index = j;
		j++;
	}
}

void
PTFFormat::parseaudio5(void) {
	uint64_t i,k,l;
	uint64_t lengthofname, wavnumber;

	// Find end of wav file list
	k = 0;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5f) &&
				(ptfunxored[k+1] == 0x50) &&
				(ptfunxored[k+2] == 0x35)) {
			break;
		}
		k++;
	}
	k++;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5f) &&
				(ptfunxored[k+1] == 0x50) &&
				(ptfunxored[k+2] == 0x35)) {
			break;
		}
		k++;
	}

	// Find actual wav names
	uint16_t numberofwavs = ptfunxored[k-23];
	char wavname[256];
	for (i = k; i < len; i++) {
		if (		(ptfunxored[i  ] == 'F') &&
				(ptfunxored[i+1] == 'i') &&
				(ptfunxored[i+2] == 'l') &&
				(ptfunxored[i+3] == 'e') &&
				(ptfunxored[i+4] == 's')) {
			break;
		}
	}

	wavnumber = 0;
	i+=16;
	char ext[5];
	while (i < len && numberofwavs > 0) {
		i++;
		if (		(ptfunxored[i  ] == 0x5a) &&
				(ptfunxored[i+1] == 0x00) &&
				(ptfunxored[i+2] == 0x05)) {
			break;
		}
		lengthofname = ptfunxored[i];
		i++;
		l = 0;
		while (l < lengthofname) {
			wavname[l] = ptfunxored[i+l];
			l++;
		}
		i+=lengthofname;
		ext[0] = ptfunxored[i++];
		ext[1] = ptfunxored[i++];
		ext[2] = ptfunxored[i++];
		ext[3] = ptfunxored[i++];
		ext[4] = '\0';

		wavname[l] = 0;
		if (foundin(wavname, ".L") || foundin(wavname, ".R")) {
			extension = string("");
		} else if (foundin(wavname, ".wav") || foundin(ext, "WAVE")) {
			extension = string(".wav");
		} else if (foundin(wavname, ".aif") || foundin(ext, "AIFF")) {
			extension = string(".aif");
		} else {
			extension = string("");
		}

		std::string wave = string(wavname);
		wav_t f = { wave, (uint16_t)(wavnumber++), 0, 0 };

		if (foundin(wave, string(".grp"))) {
			continue;
		}

		actualwavs.push_back(f);
		audiofiles.push_back(f);
		//printf("done\n");
		numberofwavs--;
		i += 7;
	}
	resort(actualwavs);
	resort(audiofiles);
}

void
PTFFormat::parseaudio(void) {
	uint64_t i,j,k,l;

	// Find end of wav file list
	k = 0;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0xff) &&
				(ptfunxored[k+1] == 0xff) &&
				(ptfunxored[k+2] == 0xff) &&
				(ptfunxored[k+3] == 0xff)) {
			break;
		}
		k++;
	}

	// Find actual wav names
	bool first = true;
	uint16_t numberofwavs;
	char wavname[256];
	for (i = k; i > 4; i--) {
		if (		((ptfunxored[i  ] == 'W') || (ptfunxored[i  ] == 'A')) &&
				((ptfunxored[i-1] == 'A') || (ptfunxored[i-1] == 'I')) &&
				((ptfunxored[i-2] == 'V') || (ptfunxored[i-2] == 'F')) &&
				((ptfunxored[i-3] == 'E') || (ptfunxored[i-3] == 'F'))) {
			j = i-4;
			l = 0;
			while (ptfunxored[j] != '\0') {
				wavname[l] = ptfunxored[j];
				l++;
				j--;
			}
			wavname[l] = 0;
			if (ptfunxored[i] == 'W') {
				extension = string(".wav");
			} else {
				extension = string(".aif");
			}
			//uint8_t playlist = ptfunxored[j-8];

			if (first) {
				first = false;
				for (j = k; j > 4; j--) {
					if (	(ptfunxored[j  ] == 0x01) &&
						(ptfunxored[j-1] == 0x5a)) {

						numberofwavs = 0;
						numberofwavs |= (uint32_t)(ptfunxored[j-2] << 24);
						numberofwavs |= (uint32_t)(ptfunxored[j-3] << 16);
						numberofwavs |= (uint32_t)(ptfunxored[j-4] << 8);
						numberofwavs |= (uint32_t)(ptfunxored[j-5]);
						//printf("%d wavs\n", numberofwavs);
						break;
					}
				k--;
				}
			}

			std::string wave = string(wavname);
			std::reverse(wave.begin(), wave.end());
			wav_t f = { wave, (uint16_t)(numberofwavs - 1), 0, 0 };

			if (foundin(wave, string(".grp"))) {
				continue;
			}

			actualwavs.push_back(f);

			numberofwavs--;
			if (numberofwavs <= 0)
				break;
		}
	}
}

void
PTFFormat::parserest89(void) {
	uint64_t i,j,k,l;
	// Find Regions
	uint8_t startbytes = 0;
	uint8_t lengthbytes = 0;
	uint8_t offsetbytes = 0;
	uint8_t somethingbytes = 0;
	uint8_t skipbytes = 0;

	k = 0;
	while (k < len) {
		if (		(ptfunxored[k  ] == 'S') &&
				(ptfunxored[k+1] == 'n') &&
				(ptfunxored[k+2] == 'a') &&
				(ptfunxored[k+3] == 'p')) {
			break;
		}
		k++;
	}
	uint16_t rindex = 0;
	uint32_t findex = 0;
	for (i = k; i < len-70; i++) {
		if (		(ptfunxored[i  ] == 0x5a) &&
				(ptfunxored[i+1] == 0x0a)) {
				break;
		}
		if (		(ptfunxored[i  ] == 0x5a) &&
				(ptfunxored[i+1] == 0x0c)) {

			uint8_t lengthofname = ptfunxored[i+9];

			char name[256] = {0};
			for (j = 0; j < lengthofname; j++) {
				name[j] = ptfunxored[i+13+j];
			}
			name[j] = '\0';
			j += i+13;
			//uint8_t disabled = ptfunxored[j];

			offsetbytes = (ptfunxored[j+1] & 0xf0) >> 4;
			lengthbytes = (ptfunxored[j+2] & 0xf0) >> 4;
			startbytes = (ptfunxored[j+3] & 0xf0) >> 4;
			somethingbytes = (ptfunxored[j+3] & 0xf);
			skipbytes = ptfunxored[j+4];
			findex = ptfunxored[j+5
					+startbytes
					+lengthbytes
					+offsetbytes
					+somethingbytes
					+skipbytes
					+40];
			/*rindex = ptfunxored[j+5
					+startbytes
					+lengthbytes
					+offsetbytes
					+somethingbytes
					+skipbytes
					+24];
			*/
			uint32_t sampleoffset = 0;
			switch (offsetbytes) {
			case 4:
				sampleoffset |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				sampleoffset |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				sampleoffset |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				sampleoffset |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=offsetbytes;
			uint32_t length = 0;
			switch (lengthbytes) {
			case 4:
				length |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				length |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				length |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				length |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=lengthbytes;
			uint32_t start = 0;
			switch (startbytes) {
			case 4:
				start |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				start |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				start |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				start |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=startbytes;
			/*
			uint32_t something = 0;
			switch (somethingbytes) {
			case 4:
				something |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				something |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				something |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				something |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=somethingbytes;
			*/
			std::string filename = string(name) + extension;
			wav_t f = {
				filename,
				0,
				(int64_t)(start*ratefactor),
				(int64_t)(length*ratefactor),
			};

			f.index = findex;
			//printf("something=%d\n", something);

			vector<wav_t>::iterator begin = actualwavs.begin();
			vector<wav_t>::iterator finish = actualwavs.end();
			vector<wav_t>::iterator found;
			// Add file to list only if it is an actual wav
			if ((found = std::find(begin, finish, f)) != finish) {
				audiofiles.push_back(f);
				// Also add plain wav as region
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f
				};
				regions.push_back(r);
			// Region only
			} else {
				if (foundin(filename, string(".grp"))) {
					continue;
				}
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f
				};
				regions.push_back(r);
			}
			rindex++;
		}
	}

	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5a) &&
				(ptfunxored[k+1] == 0x03)) {
				break;
		}
		k++;
	}
	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5a) &&
				(ptfunxored[k+1] == 0x02)) {
				break;
		}
		k++;
	}
	k++;

	//  Tracks
	uint32_t offset;
	uint32_t tracknumber = 0;
	uint32_t regionspertrack = 0;
	for (;k < len; k++) {
		if (	(ptfunxored[k  ] == 0x5a) &&
			(ptfunxored[k+1] == 0x04)) {
			break;
		}
		if (	(ptfunxored[k  ] == 0x5a) &&
			(ptfunxored[k+1] == 0x02)) {

			uint8_t lengthofname = 0;
			lengthofname = ptfunxored[k+9];
			if (lengthofname == 0x5a) {
				continue;
			}
			track_t tr;

			regionspertrack = (uint8_t)(ptfunxored[k+13+lengthofname]);

			//printf("regions/track=%d\n", regionspertrack);
			char name[256] = {0};
			for (j = 0; j < lengthofname; j++) {
				name[j] = ptfunxored[j+k+13];
			}
			name[j] = '\0';
			tr.name = string(name);
			tr.index = tracknumber++;

			for (j = k; regionspertrack > 0 && j < len; j++) {
				for (l = j; l < len; l++) {
					if (	(ptfunxored[l  ] == 0x5a) &&
						(ptfunxored[l+1] == 0x07)) {
						j = l;
						break;
					}
				}


				if (regionspertrack == 0) {
				//	tr.reg.index = (uint8_t)ptfunxored[j+13+lengthofname+5];
					break;
				} else {

					tr.reg.index = (uint8_t)(ptfunxored[l+11]);
					vector<region_t>::iterator begin = regions.begin();
					vector<region_t>::iterator finish = regions.end();
					vector<region_t>::iterator found;
					if ((found = std::find(begin, finish, tr.reg)) != finish) {
						tr.reg = (*found);
					}
					i = l+16;
					offset = 0;
					offset |= (uint32_t)(ptfunxored[i+3] << 24);
					offset |= (uint32_t)(ptfunxored[i+2] << 16);
					offset |= (uint32_t)(ptfunxored[i+1] << 8);
					offset |= (uint32_t)(ptfunxored[i]);
					tr.reg.startpos = (int64_t)(offset*ratefactor);
					if (tr.reg.length > 0) {
						tracks.push_back(tr);
					}
					regionspertrack--;
				}
			}
		}
	}
}

void
PTFFormat::parserest10(void) {
	uint64_t i,j,k,l;
	// Find Regions
	uint8_t startbytes = 0;
	uint8_t lengthbytes = 0;
	uint8_t offsetbytes = 0;
	uint8_t somethingbytes = 0;
	uint8_t skipbytes = 0;

	k = 0;
	while (k < len) {
		if (		(ptfunxored[k  ] == 'S') &&
				(ptfunxored[k+1] == 'n') &&
				(ptfunxored[k+2] == 'a') &&
				(ptfunxored[k+3] == 'p')) {
			break;
		}
		k++;
	}
	for (i = k; i < len-70; i++) {
		if (		(ptfunxored[i  ] == 0x5a) &&
				(ptfunxored[i+1] == 0x02)) {
				k = i;
				break;
		}
	}
	k++;
	for (i = k; i < len-70; i++) {
		if (		(ptfunxored[i  ] == 0x5a) &&
				(ptfunxored[i+1] == 0x02)) {
				k = i;
				break;
		}
	}
	k++;
	uint16_t rindex = 0;
	uint32_t findex = 0;
	for (i = k; i < len-70; i++) {
		if (		(ptfunxored[i  ] == 0x5a) &&
				(ptfunxored[i+1] == 0x08)) {
				break;
		}
		if (		(ptfunxored[i  ] == 0x5a) &&
				(ptfunxored[i+1] == 0x01)) {

			uint8_t lengthofname = ptfunxored[i+9];
			if (ptfunxored[i+13] == 0x5a) {
				continue;
			}
			char name[256] = {0};
			for (j = 0; j < lengthofname; j++) {
				name[j] = ptfunxored[i+13+j];
			}
			name[j] = '\0';
			j += i+13;
			//uint8_t disabled = ptfunxored[j];
			//printf("%s\n", name);

			offsetbytes = (ptfunxored[j+1] & 0xf0) >> 4;
			lengthbytes = (ptfunxored[j+2] & 0xf0) >> 4;
			startbytes = (ptfunxored[j+3] & 0xf0) >> 4;
			somethingbytes = (ptfunxored[j+3] & 0xf);
			skipbytes = ptfunxored[j+4];
			findex = ptfunxored[j+5
					+startbytes
					+lengthbytes
					+offsetbytes
					+somethingbytes
					+skipbytes
					+37];
			/*rindex = ptfunxored[j+5
					+startbytes
					+lengthbytes
					+offsetbytes
					+somethingbytes
					+skipbytes
					+24];
			*/
			uint32_t sampleoffset = 0;
			switch (offsetbytes) {
			case 4:
				sampleoffset |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				sampleoffset |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				sampleoffset |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				sampleoffset |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=offsetbytes;
			uint32_t length = 0;
			switch (lengthbytes) {
			case 4:
				length |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				length |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				length |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				length |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=lengthbytes;
			uint32_t start = 0;
			switch (startbytes) {
			case 4:
				start |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				start |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				start |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				start |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=startbytes;
			/*
			uint32_t something = 0;
			switch (somethingbytes) {
			case 4:
				something |= (uint32_t)(ptfunxored[j+8] << 24);
			case 3:
				something |= (uint32_t)(ptfunxored[j+7] << 16);
			case 2:
				something |= (uint32_t)(ptfunxored[j+6] << 8);
			case 1:
				something |= (uint32_t)(ptfunxored[j+5]);
			default:
				break;
			}
			j+=somethingbytes;
			*/
			std::string filename = string(name) + extension;
			wav_t f = {
				filename,
				0,
				(int64_t)(start*ratefactor),
				(int64_t)(length*ratefactor),
			};

			if (strlen(name) == 0) {
				continue;
			}
			if (length == 0) {
				continue;
			}
			f.index = findex;
			//printf("something=%d\n", something);

			vector<wav_t>::iterator begin = actualwavs.begin();
			vector<wav_t>::iterator finish = actualwavs.end();
			vector<wav_t>::iterator found;
			// Add file to list only if it is an actual wav
			if ((found = std::find(begin, finish, f)) != finish) {
				audiofiles.push_back(f);
				// Also add plain wav as region
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f
				};
				regions.push_back(r);
			// Region only
			} else {
				if (foundin(filename, string(".grp"))) {
					continue;
				}
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f
				};
				regions.push_back(r);
			}
			rindex++;
			//printf("%s\n", name);
		}
	}
	//  Tracks
	uint32_t offset;
	uint32_t tracknumber = 0;
	uint32_t regionspertrack = 0;
	for (;k < len; k++) {
		if (	(ptfunxored[k  ] == 0x5a) &&
			(ptfunxored[k+1] == 0x08)) {
			break;
		}
	}
	k++;
	for (;k < len; k++) {
		if (	(ptfunxored[k  ] == 0x5a) &&
			(ptfunxored[k+1] == 0x04)) {
			break;
		}
		if (	(ptfunxored[k  ] == 0x5a) &&
			(ptfunxored[k+1] == 0x02)) {

			uint8_t lengthofname = 0;
			lengthofname = ptfunxored[k+9];
			if (lengthofname == 0x5a) {
				continue;
			}
			track_t tr;

			regionspertrack = (uint8_t)(ptfunxored[k+13+lengthofname]);

			//printf("regions/track=%d\n", regionspertrack);
			char name[256] = {0};
			for (j = 0; j < lengthofname; j++) {
				name[j] = ptfunxored[j+k+13];
			}
			name[j] = '\0';
			tr.name = string(name);
			tr.index = tracknumber++;

			for (j = k; regionspertrack > 0 && j < len; j++) {
				for (l = j; l < len; l++) {
					if (	(ptfunxored[l  ] == 0x5a) &&
						(ptfunxored[l+1] == 0x08)) {
						j = l+1;
						break;
					}
				}


				if (regionspertrack == 0) {
				//	tr.reg.index = (uint8_t)ptfunxored[j+13+lengthofname+5];
					break;
				} else {

					tr.reg.index = (uint8_t)(ptfunxored[l+11]);
					vector<region_t>::iterator begin = regions.begin();
					vector<region_t>::iterator finish = regions.end();
					vector<region_t>::iterator found;
					if ((found = std::find(begin, finish, tr.reg)) != finish) {
						tr.reg = (*found);
					}
					i = l+16;
					offset = 0;
					offset |= (uint32_t)(ptfunxored[i+3] << 24);
					offset |= (uint32_t)(ptfunxored[i+2] << 16);
					offset |= (uint32_t)(ptfunxored[i+1] << 8);
					offset |= (uint32_t)(ptfunxored[i]);
					tr.reg.startpos = (int64_t)(offset*ratefactor);
					if (tr.reg.length > 0) {
						tracks.push_back(tr);
					}
					regionspertrack--;
				}
			}
		}
	}
}
