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
#ifndef PTFFORMAT_H
#define PTFFORMAT_H

#include <string>
#include <algorithm>
#include <vector>
#include <stdint.h>

class PTFFormat {
public:
	PTFFormat();
	~PTFFormat();

	/* Return values:	0            success
				-1           could not open file as ptf
	*/
	int load(std::string path, int64_t targetsr);

	typedef struct wav {
		std::string filename;
		uint16_t    index;

		int64_t     posabsolute;
		int64_t     length;

		bool operator ==(const struct wav& other) {
			return (this->filename == other.filename ||
				this->index == other.index);
		}

	} wav_t;

	typedef struct region {
		std::string name;
		uint16_t    index;
		int64_t     startpos;
		int64_t     sampleoffset;
		int64_t     length;
		wav_t       wave;

		bool operator ==(const struct region& other) {
			return (this->index == other.index);
		}
	} region_t;

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
	std::vector<track_t> tracks;

	static bool regionexistsin(std::vector<region_t> reg, uint16_t index) {
		std::vector<region_t>::iterator begin = reg.begin();
		std::vector<region_t>::iterator finish = reg.end();
		std::vector<region_t>::iterator found;

		wav_t w = { std::string(""), 0, 0, 0 };
		region_t r = { std::string(""), index, 0, 0, 0, w };

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

	unsigned char c0;
	unsigned char c1;
	unsigned char *ptfunxored;
	int len;

private:
	bool foundin(std::string haystack, std::string needle);
	void parse(void);
	void unxor10(void);
	void setrates(void);
	void parse5header(void);
	void parse7header(void);
	void parse8header(void);
	void parse9header(void);
	void parse10header(void);
	void parserest5(void);
	void parserest89(void);
	void parserest10(void);
	void parseaudio5(void);
	void parseaudio(void);
	std::vector<wav_t> actualwavs;
	float ratefactor;
	std::string extension;
	unsigned char key10a;
	unsigned char key10b;
};


#endif
