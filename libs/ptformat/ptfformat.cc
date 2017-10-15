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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <assert.h>

#include <glib/gstdio.h>

#include "ptfformat.h"

using namespace std;

static void
hexdump(uint8_t *data, int len)
{
	int i,j,end,step=16;

	for (i = 0; i < len; i += step) {
		printf("0x%02X: ", i);
		end = i + step;
		if (end > len) end = len;
		for (j = i; j < end; j++) {
			printf("0x%02X ", data[j]);
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

PTFFormat::PTFFormat() : version(0), product(NULL) {
}

PTFFormat::~PTFFormat() {
	if (ptfunxored) {
		free(ptfunxored);
	}
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
	uint64_t i;
	uint8_t xor_type;
	uint8_t xor_value;
	uint8_t xor_delta;
	uint16_t xor_len;
	int err;

	if (! (fp = g_fopen(path.c_str(), "rb"))) {
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	if (len < 0x14) {
		fclose(fp);
		return -1;
	}

	if (! (ptfunxored = (unsigned char*) malloc(len * sizeof(unsigned char)))) {
		/* Silently fail -- out of memory*/
		fclose(fp);
		ptfunxored = 0;
		return -1;
	}

	/* The first 20 bytes are always unencrypted */
	fseek(fp, 0x00, SEEK_SET);
	i = fread(ptfunxored, 1, 0x14, fp);
	if (i < 0x14) {
		fclose(fp);
		return -1;
	}

	xor_type = ptfunxored[0x12];
	xor_value = ptfunxored[0x13];
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
		ptfunxored[i++] = ct ^ xxor[xor_index];
	}
	fclose(fp);

	if (!parse_version())
		return -1;

	if (version < 5 || version > 12)
		return -1;

	targetrate = targetsr;
	err = parse();
	if (err)
		return -1;

	return 0;
}

bool
PTFFormat::parse_version() {
	uint32_t seg_len,str_len;
	uint8_t *data = ptfunxored + 0x14;
	uintptr_t data_end = ((uintptr_t)ptfunxored) + 0x100;
	uint8_t seg_type; 
	bool success = false;

	while( ((uintptr_t)data < data_end) && (success == false) ) {

		if (data[0] != 0x5a) {
			success = false;
			break;
		}

		seg_type = data[1];
		/* Skip segment header */
		data += 3;
		if (data[0] == 0 && data[1] == 0) {
			/* LE */
			seg_len = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
		} else {
			/* BE */
			seg_len = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];
		}
		/* Skip seg_len */
		data += 4;
		if (!(seg_type == 0x04 || seg_type == 0x03) || data[0] != 0x03) {
			/* Go to next segment */
			data += seg_len;
			continue;
		}
		/* Skip 0x03 0x00 0x00 */
		data += 3;
		seg_len -= 3;
		str_len = (*(uint8_t *)data);
		if (! (product = (uint8_t *)malloc((str_len+1) * sizeof(uint8_t)))) {
			success = false;
			break;
		}

		/* Skip str_len */
		data += 4;
		seg_len -= 4;

		memcpy(product, data, str_len);
		product[str_len] = 0;
		data += str_len;
		seg_len -= str_len;

		/* Skip 0x03 0x00 0x00 0x00 */
		data += 4;
		seg_len -= 4;

		version = data[0];
		if (version == 0) {
			version = data[3];
		}
		data += seg_len;
		success = true;
	}

	/* If the above does not work, assume old version 5,6,7 */
	if ((uintptr_t)data >= data_end - seg_len) {
		version = ptfunxored[0x40];
		success = true;
	}
	return success;
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

int
PTFFormat::parse(void) {
	if (version == 5) {
		parse5header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio5();
		parserest5();
		parsemidi();
	} else if (version == 7) {
		parse7header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio();
		parserest89();
		parsemidi();
	} else if (version == 8) {
		parse8header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio();
		parserest89();
		parsemidi();
	} else if (version == 9) {
		parse9header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio();
		parserest89();
		parsemidi();
	} else if (version == 10 || version == 11 || version == 12) {
		parse10header();
		setrates();
		if (sessionrate < 44100 || sessionrate > 192000)
		  return -1;
		parseaudio();
		parserest10();
		parsemidi();
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
				std::vector<midi_ev_t> m;
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					*found,
					m
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
				std::vector<midi_ev_t> m;
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f,
					m,
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
PTFFormat::parsemidi(void) {
	uint64_t i, k, n_midi_events, zero_ticks;
	uint64_t midi_pos, midi_len, max_pos;
	uint8_t midi_velocity, midi_note;
	uint16_t rsize;
	midi_ev_t m;
	bool found = false;
	int max_regions = regions.size();
	char midiname[26] = { 0 };

	// Find MdNLB
	k = 0;

	// Parse all midi tracks, treat each group of midi bytes as a track
	while (k + 35 < len) {
		max_pos = 0;
		std::vector<midi_ev_t> midi;

		while (k < len) {
			if (		(ptfunxored[k  ] == 'M') &&
					(ptfunxored[k+1] == 'd') &&
					(ptfunxored[k+2] == 'N') &&
					(ptfunxored[k+3] == 'L') &&
					(ptfunxored[k+4] == 'B')) {
				found = true;
				break;
			}
			k++;
		}

		if (!found) {
			return;
		}

		k += 11;
		n_midi_events = ptfunxored[k] | ptfunxored[k+1] << 8 |
				ptfunxored[k+2] << 16 | ptfunxored[k+3] << 24;

		k += 4;
		zero_ticks = (uint64_t)ptfunxored[k] |
				(uint64_t)ptfunxored[k+1] << 8 |
				(uint64_t)ptfunxored[k+2] << 16 |
				(uint64_t)ptfunxored[k+3] << 24 |
				(uint64_t)ptfunxored[k+4] << 32;
		for (i = 0; i < n_midi_events && k < len; i++, k += 35) {
			midi_pos = (uint64_t)ptfunxored[k] |
				(uint64_t)ptfunxored[k+1] << 8 |
				(uint64_t)ptfunxored[k+2] << 16 |
				(uint64_t)ptfunxored[k+3] << 24 |
				(uint64_t)ptfunxored[k+4] << 32;
			midi_pos -= zero_ticks;
			midi_note = ptfunxored[k+8];
			midi_len = (uint64_t)ptfunxored[k+9] |
				(uint64_t)ptfunxored[k+10] << 8 |
				(uint64_t)ptfunxored[k+11] << 16 |
				(uint64_t)ptfunxored[k+12] << 24 |
				(uint64_t)ptfunxored[k+13] << 32;
			midi_velocity = ptfunxored[k+17];

			if (midi_pos + midi_len > max_pos) {
				max_pos = midi_pos + midi_len;
			}

			m.pos = midi_pos;
			m.length = midi_len;
			m.note = midi_note;
			m.velocity = midi_velocity;
#if 1
// stop gap measure to prevent crashes in ardour,
// remove when decryption is fully solved for .ptx
			if ((m.velocity & 0x80) || (m.note & 0x80) ||
					(m.pos & 0xff00000000LL) || (m.length & 0xff00000000LL)) {
				continue;
			}
#endif
			midi.push_back(m);

			//fprintf(stderr, "MIDI:  Note=%d Vel=%d Start=%d(samples) Len=%d(samples)\n", midi_note, midi_velocity, midi_pos, midi_len);
		}

		rsize = (uint16_t)regions.size();
		snprintf(midiname, 20, "MIDI-%d", rsize - max_regions + 1);
		wav_t w = { std::string(""), 0, 0, 0 };
		region_t r = {
			midiname,
			rsize,
			(int64_t)(0),
			(int64_t)(0),
			(int64_t)(max_pos*sessionrate*60/(960000*120)),
			w,
			midi,
		};
		regions.push_back(r);
	}
}

void
PTFFormat::parseaudio(void) {
	uint64_t i,j,k,l;
	int64_t index = foundat(ptfunxored, len, "Audio Files");

	if (index < 0)
		return;

	// Find end of wav file list
	k = (uint64_t)index;
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
	for (i = k-2; i > 4; i--) {
		if (		((ptfunxored[i  ] == 'W') || (ptfunxored[i  ] == 'A') || ptfunxored[i  ] == '\0') &&
				((ptfunxored[i-1] == 'A') || (ptfunxored[i-1] == 'I') || ptfunxored[i-1] == '\0') &&
				((ptfunxored[i-2] == 'V') || (ptfunxored[i-2] == 'F') || ptfunxored[i-2] == '\0') &&
				((ptfunxored[i-3] == 'E') || (ptfunxored[i-3] == 'F') || ptfunxored[i-3] == '\0')) {
			j = i-4;
			l = 0;
			while (ptfunxored[j] != '\0') {
				wavname[l] = ptfunxored[j];
				l++;
				j--;
			}
			wavname[l] = 0;
			if (ptfunxored[i] == 'A') {
				extension = string(".aif");
			} else {
				extension = string(".wav");
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
				std::vector<midi_ev_t> m;
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f,
					m
				};
				regions.push_back(r);
			// Region only
			} else {
				if (foundin(filename, string(".grp"))) {
					continue;
				}
				std::vector<midi_ev_t> m;
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f,
					m
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
				std::vector<midi_ev_t> m;
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f,
					m
				};
				regions.push_back(r);
			// Region only
			} else {
				if (foundin(filename, string(".grp"))) {
					continue;
				}
				std::vector<midi_ev_t> m;
				region_t r = {
					name,
					rindex,
					(int64_t)(start*ratefactor),
					(int64_t)(sampleoffset*ratefactor),
					(int64_t)(length*ratefactor),
					f,
					m
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
