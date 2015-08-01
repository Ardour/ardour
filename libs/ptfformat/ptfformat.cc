/*
    Copyright (C) 2015  Damien Zammit

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
using namespace std;

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
PTFFormat::load(std::string path) {
	FILE *fp;
	unsigned char xxor[256];
	unsigned char ct;
	unsigned char px;
	uint16_t i;
	int j;
	int li;
	int inv;
	unsigned char message;

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

	if (! (ptfunxored = (unsigned char*) malloc(len * sizeof(unsigned char)))) {
		/* Silently fail -- out of memory*/
		fclose(fp);
		ptfunxored = 0;
		return -1;
	}

	i = 2;

	fseek(fp, 0x0, SEEK_SET);

	switch (c0) {
	case 0x00:
		// Success! easy one
		xxor[0] = c0;
		xxor[1] = c1;
		for (i = 2; i < 64; i++) {
			xxor[i] = (xxor[i-1] + c1 - c0) & 0xff;
		}
		px = xxor[0];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[0] = message;
		px  = xxor[1];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[1] = message;
		i = 2;
		j = 2;
		while (fread(&ct, 1, 1, fp) != 0) {
			if (i%64 == 0) {
				i = 0;
			}
			message = xxor[i] ^ ct;
			ptfunxored[j] = message;
			i++;
			j++;
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
		px = xxor[0];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[0] = message;
		px  = xxor[1];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[1] = message;
		i = 2;
		j = 2;
		while (fread(&ct, 1, 1, fp) != 0) {
			if (i%256 == 0) {
				i = 0;
			}
			message = xxor[i] ^ ct;
			ptfunxored[j] = message;
			i++;
			j++;
		}
		break;
	case 0x40:
	case 0xc0:
		li = c1;
		if (ptflutseenwild[li]) {
			//Success! [li]
		} else {
			//Can't find lookup table for c1=li
			free(ptfunxored);
			fclose(fp);
			return li;
		}
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
			xxor[i] ^= (ptflut[li][i] * 0x40);
		}
		inv = 0;
		for (i = 128; i < 192; i++) {
			inv = (ptflut[li][i-128] == 3) ? 1 : 3;
			xxor[i] ^= (inv * 0x40);
		}
		for (i = 192; i < 256; i++) {
			xxor[i] ^= 0x80;
		}
		px = xxor[0];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[0] = message;
		px  = xxor[1];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[1] = message;
		i = 2;
		j = 2;
		while (fread(&ct, 1, 1, fp) != 0) {
			if (i%256 == 0) {
				i = 0;
			}
			message = xxor[i] ^ ct;
			ptfunxored[j] = message;
			i++;
			j++;
		}
		break;
		break;
	default:
		//Should not happen, failed c[0] c[1]
		return -1;
		break;
	}
	fclose(fp);

	parse();
	return 0;
}

void
PTFFormat::parse(void) {
	int i;
	int j;
	int k;
	int l;

	// Find session sample rate
	k = 0;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5a) &&
				(ptfunxored[k+1] == 0x05)) {
			break;
		}
		k++;
	}
	this->sessionrate = 0;
	this->sessionrate |= ptfunxored[k+11];
	this->sessionrate |= ptfunxored[k+12] << 8;
	this->sessionrate |= ptfunxored[k+13] << 16;

	// Find end of wav file list
	while (k < len) {
		if (		(ptfunxored[k  ] == 0xff) &&
				(ptfunxored[k+1] == 0xff) &&
				(ptfunxored[k+2] == 0xff) &&
				(ptfunxored[k+3] == 0xff)) {
			break;
		}
		k++;
	}

	j = 0;
	l = 0;

	// Find actual wav names
	bool first = true;
	uint16_t numberofwavs;
	char wavname[256];
	for (i = k; i > 4; i--) {
		if (		(ptfunxored[i  ] == 'W') &&
				(ptfunxored[i-1] == 'A') &&
				(ptfunxored[i-2] == 'V') &&
				(ptfunxored[i-3] == 'E')) {
			j = i-4;
			l = 0;
			while (ptfunxored[j] != '\0') {
				wavname[l] = ptfunxored[j];
				l++;
				j--;
			}
			wavname[l] = 0;
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
			wav_t f = { wave, (uint16_t)(numberofwavs - 1), 0 };

			if (foundin(wave, string(".grp"))) {
				continue;
			}

			actualwavs.push_back(f);

			numberofwavs--;
			if (numberofwavs <= 0)
				break;
		}
	}

	// Find Regions
	uint8_t startbytes = 0;
	uint8_t lengthbytes = 0;
	uint8_t offsetbytes = 0;
	uint8_t somethingbytes = 0;
	uint8_t skipbytes = 0;

	while (k < len) {
		if (		(ptfunxored[k  ] == 'S') &&
				(ptfunxored[k+1] == 'n') &&
				(ptfunxored[k+2] == 'a') &&
				(ptfunxored[k+3] == 'p')) {
			break;
		}
		k++;
	}
	first = true;
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
			std::string filename = string(name) + ".wav";
			wav_t f = { 
				filename,
				0,
				(int64_t)start,
				(int64_t)length,
			};

			f.index = findex;
			//printf("something=%d\n", something);

			vector<wav_t>::iterator begin = this->actualwavs.begin();
			vector<wav_t>::iterator finish = this->actualwavs.end();
			vector<wav_t>::iterator found;
			// Add file to list only if it is an actual wav
			if ((found = std::find(begin, finish, f)) != finish) {
				this->audiofiles.push_back(f);
				// Also add plain wav as region
				region_t r = {
					name,
					rindex,
					start,
					sampleoffset,
					length,
					f
				};
				this->regions.push_back(r);
			// Region only
			} else {
				if (foundin(filename, string(".grp"))) {
					continue;
				}
				region_t r = {
					name,
					rindex,
					start,
					sampleoffset,
					length,
					f
				};
				this->regions.push_back(r);
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
					vector<region_t>::iterator begin = this->regions.begin();
					vector<region_t>::iterator finish = this->regions.end();
					vector<region_t>::iterator found;
					if ((found = std::find(begin, finish, tr.reg)) != finish) {
						tr.reg = (*found);
					}
					startbytes = (ptfunxored[l+3] & 0xf0) >> 4;

					i = l+16;
					offset = 0;
					switch (startbytes) {
					case 4:
						offset |= (uint32_t)(ptfunxored[i+3] << 24);
					case 3:
						offset |= (uint32_t)(ptfunxored[i+2] << 16);
					case 2:
						offset |= (uint32_t)(ptfunxored[i+1] << 8);
					case 1:
						offset |= (uint32_t)(ptfunxored[i]);
					default:
						break;
					}
					tr.reg.startpos = (int64_t)offset;
					if (tr.reg.length > 0) {
						this->tracks.push_back(tr);
					}
					regionspertrack--;
				}
			}
		}
	}
}
