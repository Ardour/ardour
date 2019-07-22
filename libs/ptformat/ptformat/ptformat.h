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
				-1           error decrypting pt session
				-2           error detecting pt session
				-3           incompatible pt version
				-4           error parsing pt session
	*/
	int load(std::string const& path, int64_t targetsr);

	/* Return values:	0            success
				-1           error decrypting pt session
	*/
	int unxor(std::string const& path);

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

		wav_t (uint16_t idx = 0) : index (idx), posabsolute (0), length (0) {}
	};

	struct midi_ev_t {
		uint64_t pos;
		uint64_t length;
		uint8_t note;
		uint8_t velocity;
		midi_ev_t () : pos (0), length (0), note (0), velocity (0) {}
	};

	struct region_t {
		std::string name;
		uint16_t    index;
		int64_t     startpos;
		int64_t     sampleoffset;
		int64_t     length;
		wav_t       wave;
		std::vector<midi_ev_t> midi;

		bool operator ==(const region_t& other) const {
			return (this->index == other.index);
		}

		bool operator <(const region_t& other) const {
			return (strcasecmp(this->name.c_str(),
					other.name.c_str()) < 0);
		}
		region_t (uint16_t idx = 0) : index (idx), startpos (0), sampleoffset (0), length (0) {}
	};

	struct track_t {
		std::string name;
		uint16_t    index;
		uint8_t     playlist;
		region_t    reg;

		bool operator <(const track_t& other) const {
			return (this->index < other.index);
		}

		bool operator ==(const track_t& other) const {
			return (this->index == other.index);
		}
		track_t (uint16_t idx = 0) : index (idx), playlist (0) {}
	};

	bool find_track(uint16_t index, track_t& tt) const {
		std::vector<track_t>::const_iterator begin = _tracks.begin();
		std::vector<track_t>::const_iterator finish = _tracks.end();
		std::vector<track_t>::const_iterator found;

		track_t t (index);

		if ((found = std::find(begin, finish, t)) != finish) {
			tt = *found;
			return true;
		}
		return false;
	}

	bool find_region(uint16_t index, region_t& rr) const {
		std::vector<region_t>::const_iterator begin = _regions.begin();
		std::vector<region_t>::const_iterator finish = _regions.end();
		std::vector<region_t>::const_iterator found;

		region_t r;
		r.index = index;

		if ((found = std::find(begin, finish, r)) != finish) {
			rr = *found;
			return true;
		}
		return false;
	}
	
	bool find_miditrack(uint16_t index, track_t& tt) const {
		std::vector<track_t>::const_iterator begin = _miditracks.begin();
		std::vector<track_t>::const_iterator finish = _miditracks.end();
		std::vector<track_t>::const_iterator found;

		track_t t (index);

		if ((found = std::find(begin, finish, t)) != finish) {
			tt = *found;
			return true;
		}
		return false;
	}

	bool find_midiregion(uint16_t index, region_t& rr) const {
		std::vector<region_t>::const_iterator begin = _midiregions.begin();
		std::vector<region_t>::const_iterator finish = _midiregions.end();
		std::vector<region_t>::const_iterator found;

		region_t r (index);

		if ((found = std::find(begin, finish, r)) != finish) {
			rr = *found;
			return true;
		}
		return false;
	}

	bool find_wav(uint16_t index, wav_t& ww) const {
		std::vector<wav_t>::const_iterator begin = _audiofiles.begin();
		std::vector<wav_t>::const_iterator finish = _audiofiles.end();
		std::vector<wav_t>::const_iterator found;

		wav_t w (index);

		if ((found = std::find(begin, finish, w)) != finish) {
			ww = *found;
			return true;
		}
		return false;
	}

	static bool regionexistsin(std::vector<region_t> const& reg, uint16_t index) {
		std::vector<region_t>::const_iterator begin = reg.begin();
		std::vector<region_t>::const_iterator finish = reg.end();

		region_t r (index);

		if (std::find(begin, finish, r) != finish) {
			return true;
		}
		return false;
	}

	static bool wavexistsin (std::vector<wav_t> const& wv, uint16_t index) {
		std::vector<wav_t>::const_iterator begin = wv.begin();
		std::vector<wav_t>::const_iterator finish = wv.end();

		wav_t w (index);

		if (std::find(begin, finish, w) != finish) {
			return true;
		}
		return false;
	}

	uint8_t version () const { return _version; }
	int64_t sessionrate () const { return _sessionrate ; }
	const std::string& path () { return _path; }

	const std::vector<wav_t>&    audiofiles () const { return _audiofiles ; }
	const std::vector<region_t>& regions () const { return _regions ; }
	const std::vector<region_t>& midiregions () const { return _midiregions ; }
	const std::vector<track_t>&  tracks () const { return _tracks ; }
	const std::vector<track_t>&  miditracks () const { return _miditracks ; }

	const unsigned char* unxored_data () const { return _ptfunxored; }
	uint64_t             unxored_size () const { return _len; }

private:

	std::vector<wav_t>    _audiofiles;
	std::vector<region_t> _regions;
	std::vector<region_t> _midiregions;
	std::vector<track_t>  _tracks;
	std::vector<track_t>  _miditracks;

	std::string _path;

	unsigned char* _ptfunxored;
	uint64_t       _len;
	int64_t        _sessionrate;
	uint8_t        _version;
	uint8_t*       _product;
	int64_t        _targetrate;
	float          _ratefactor;
	bool           is_bigendian;

	struct block_t {
		uint8_t zmark;			// 'Z'
		uint16_t block_type;		// type of block
		uint32_t block_size;		// size of block
		uint16_t content_type;		// type of content
		uint32_t offset;		// offset in file
		std::vector<block_t> child;	// vector of child blocks
	};
	std::vector<block_t> blocks;

	bool jumpback(uint32_t *currpos, unsigned char *buf, const uint32_t maxoffset, const unsigned char *needle, const uint32_t needlelen);
	bool jumpto(uint32_t *currpos, unsigned char *buf, const uint32_t maxoffset, const unsigned char *needle, const uint32_t needlelen);
	bool foundin(std::string const& haystack, std::string const& needle);
	int64_t foundat(unsigned char *haystack, uint64_t n, const char *needle);

	std::string parsestring(uint32_t pos);
	const std::string get_content_description(uint16_t ctype);
	int parse(void);
	void parseblocks(void);
	bool parseheader(void);
	bool parserest(void);
	bool parseaudio(void);
	bool parsemidi(void);
	void dump(void);
	bool parse_block_at(uint32_t pos, struct block_t *b, struct block_t *parent, int level);
	void dump_block(struct block_t& b, int level);
	bool parse_version();
	void parse_region_info(uint32_t j, block_t& blk, region_t& r);
	void parse_three_point(uint32_t j, uint64_t& start, uint64_t& offset, uint64_t& length);
	uint8_t gen_xor_delta(uint8_t xor_value, uint8_t mul, bool negative);
	void setrates(void);
	void cleanup(void);
	void free_block(struct block_t& b);
	void free_all_blocks(void);
};

#endif
