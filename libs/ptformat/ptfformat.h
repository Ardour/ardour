/*
 * libptformat - a library to read ProTools sessions
 *
 * Copyright (C) 2015  Damien Zammit
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
#ifndef PTFFORMAT_H
#define PTFFORMAT_H

#include <string>
#include <cstring>
#include <algorithm>
#include <vector>
#include <stdint.h>
#include "ptformat/visibility.h"

class LIBPTFORMAT_API PTFFormat {
public:
	PTFFormat();
	~PTFFormat();

	/* Return values:	0            success
				-1           could not parse pt session
	*/
	int load(std::string path, int64_t targetsr);

	/* Return values:	0            success
				-1           could not decrypt pt session
	*/
	int unxor(std::string path);

	struct wav_t {
		std::string filename;
		uint16_t    index;

		int64_t     posabsolute;
		int64_t     length;

		bool operator <(const struct wav_t& other) const {
			return (strcasecmp(this->filename.c_str(),
					other.filename.c_str()) < 0);
		}

		bool operator ==(const struct wav_t& other) const {
			return (this->filename == other.filename ||
				this->index == other.index);
		}

	};

	struct midi_ev_t {
		uint64_t pos;
		uint64_t length;
		uint8_t note;
		uint8_t velocity;
	};

	typedef struct region {
		std::string name;
		uint16_t    index;
		int64_t     startpos;
		int64_t     sampleoffset;
		int64_t     length;
		wav_t       wave;
		std::vector<midi_ev_t> midi;

		bool operator ==(const struct region& other) {
			return (this->index == other.index);
		}

		bool operator <(const struct region& other) const {
			return (strcasecmp(this->name.c_str(),
					other.name.c_str()) < 0);
		}
	} region_t;

	typedef struct compound {
		uint16_t curr_index;
		uint16_t unknown1;
		uint16_t level;
		uint16_t ontopof_index;
		uint16_t next_index;
		std::string name;
	} compound_t;

	typedef struct track {
		std::string name;
		uint16_t    index;
		uint8_t     playlist;
		region_t    reg;

		bool operator ==(const struct track& other) {
			return (this->name == other.name);
		}
	} track_t;

	std::vector<wav_t> audiofiles;
	std::vector<region_t> regions;
	std::vector<region_t> midiregions;
	std::vector<compound_t> compounds;
	std::vector<track_t> tracks;
	std::vector<track_t> miditracks;

	static bool regionexistsin(std::vector<region_t> reg, uint16_t index) {
		std::vector<region_t>::iterator begin = reg.begin();
		std::vector<region_t>::iterator finish = reg.end();
		std::vector<region_t>::iterator found;

		wav_t w = { std::string(""), 0, 0, 0 };
		std::vector<midi_ev_t> m;
		region_t r = { std::string(""), index, 0, 0, 0, w, m};

		if ((found = std::find(begin, finish, r)) != finish) {
			return true;
		}
		return false;
	}

	static bool wavexistsin(std::vector<wav_t> wv, uint16_t index) {
		std::vector<wav_t>::iterator begin = wv.begin();
		std::vector<wav_t>::iterator finish = wv.end();
		std::vector<wav_t>::iterator found;

		wav_t w = { std::string(""), index, 0, 0 };

		if ((found = std::find(begin, finish, w)) != finish) {
			return true;
		}
		return false;
	}

	int64_t sessionrate;
	int64_t targetrate;
	uint8_t version;
	uint8_t *product;


	unsigned char c0;
	unsigned char c1;
	unsigned char *ptfunxored;
	uint64_t len;

private:
	bool jumpback(uint32_t *currpos, unsigned char *buf, const uint32_t maxoffset, const unsigned char *needle, const uint32_t needlelen);
	bool jumpto(uint32_t *currpos, unsigned char *buf, const uint32_t maxoffset, const unsigned char *needle, const uint32_t needlelen);
	bool foundin(std::string haystack, std::string needle);
	int64_t foundat(unsigned char *haystack, uint64_t n, const char *needle);
	int parse(void);
	bool parse_version();
	uint8_t gen_xor_delta(uint8_t xor_value, uint8_t mul, bool negative);
	void setrates(void);
	void parse5header(void);
	void parse7header(void);
	void parse8header(void);
	void parse9header(void);
	void parse10header(void);
	void parserest5(void);
	void parserest89(void);
	void parserest12(void);
	void parseaudio5(void);
	void parseaudio(void);
	void parsemidi(void);
	void parsemidi12(void);
	void resort(std::vector<wav_t>& ws);
	void resort(std::vector<region_t>& rs);
	void filter(std::vector<region_t>& rs);
	std::vector<wav_t> actualwavs;
	float ratefactor;
	std::string extension;
};


#endif
