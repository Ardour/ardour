/* This file is part of Evoral.
 * Copyright(C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright(C) 2000-2008 Paul Davis
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or(at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <cstring>
#include <cstdio>
#include <cassert>
#include <iostream>
#include <glibmm/miscutils.h>

#include "evoral/midi_util.h"
#include "evoral/SMFReader.hpp"

using namespace std;

namespace Evoral {


SMFReader::SMFReader(const string& filename)
	: _fd(NULL)
	, _ppqn(0)
	, _track(0)
	, _track_size(0)
{
	if (filename.length() > 0) {
		open(filename);
	}
}


SMFReader::~SMFReader()
{
	if (_fd)
		close();
}


bool
SMFReader::open(const string& filename) throw (logic_error, UnsupportedTime)
{
	if (_fd)
		throw logic_error("Attempt to start new read while write in progress.");

	cout << "Opening SMF file " << filename << " for reading." << endl;

	_fd = fopen(filename.c_str(), "r+");

	if (_fd) {
		// Read type (bytes 8..9)
		fseek(_fd, 0, SEEK_SET);
		char mthd[5];
		mthd[4] = '\0';
		fread(mthd, 1, 4, _fd);
		if (strcmp(mthd, "MThd")) {
			cerr << filename << " is not an SMF file, aborting." << endl;
			fclose(_fd);
			_fd = NULL;
			return false;
		}
		
		// Read type (bytes 8..9)
		fseek(_fd, 8, SEEK_SET);
		uint16_t type_be = 0;
		fread(&type_be, 2, 1, _fd);
		_type = GUINT16_FROM_BE(type_be);

		// Read number of tracks (bytes 10..11)
		uint16_t num_tracks_be = 0;
		fread(&num_tracks_be, 2, 1, _fd);
		_num_tracks = GUINT16_FROM_BE(num_tracks_be);

		// Read PPQN (bytes 12..13)
		uint16_t ppqn_be = 0;
		fread(&ppqn_be, 2, 1, _fd);
		_ppqn = GUINT16_FROM_BE(ppqn_be);
		
		// TODO: Absolute (SMPTE seconds) time support
		if ((_ppqn & 0x8000) != 0)
			throw UnsupportedTime();

		seek_to_track(1);
		
		return true;
	} else {
		return false;
	}
}

	
/** Seek to the start of a given track, starting from 1.
 * Returns true if specified track was found.
 */
bool
SMFReader::seek_to_track(unsigned track) throw (std::logic_error)
{
	if (track == 0)
		throw logic_error("Seek to track 0 out of range (must be >= 1)");

	if (!_fd)
		throw logic_error("Attempt to seek to track on unopened SMF file.");

	unsigned track_pos = 0;

	fseek(_fd, 14, SEEK_SET);
	char id[5];
	id[4] = '\0';
	uint32_t chunk_size = 0;

	while (!feof(_fd)) {
		fread(id, 1, 4, _fd);

		if (!strcmp(id, "MTrk")) {
			++track_pos;
		} else {
			std::cerr << "Unknown chunk ID " << id << endl;
		}

		uint32_t chunk_size_be;
		fread(&chunk_size_be, 4, 1, _fd);
		chunk_size = GUINT32_FROM_BE(chunk_size_be);

		if (track_pos == track)
			break;

		fseek(_fd, chunk_size, SEEK_CUR);
	}

	if (!feof(_fd) && track_pos == track) {
		_track = track;
		_track_size = chunk_size;
		return true;
	} else {
		return false;
	}
}

	
/** Read an event from the current position in file.
 *
 * File position MUST be at the beginning of a delta time, or this will die very messily.
 * ev.buffer must be of size ev.size, and large enough for the event.  The returned event
 * will have it's time field set to it's delta time (so it's the caller's responsibility
 * to keep track of delta time, even for ignored events).
 *
 * Returns event length (including status byte) on success, 0 if event was
 * skipped (eg a meta event), or -1 on EOF (or end of track).
 *
 * If @a buf is not large enough to hold the event, 0 will be returned, but ev_size
 * set to the actual size of the event.
 */
int
SMFReader::read_event(size_t    buf_len,
                      uint8_t*  buf,
                      uint32_t* ev_size,
                      uint32_t* delta_time)
		throw (std::logic_error, PrematureEOF, CorruptFile)
{
	if (_track == 0)
		throw logic_error("Attempt to read from unopened SMF file");

	if (!_fd || feof(_fd)) {
		return -1;
	}

	assert(buf_len > 0);
	assert(buf);
	assert(ev_size);
	assert(delta_time);

	// Running status state
	static uint8_t  last_status = 0;
	static uint32_t last_size   = 0;

	*delta_time = read_var_len(_fd);
	int status = fgetc(_fd);
	if (status == EOF)
		throw PrematureEOF();
	else if (status > 0xFF)
		throw CorruptFile();

	if (status < 0x80) {
		if (last_status == 0)
			throw CorruptFile();
		status = last_status;
		*ev_size = last_size;
		fseek(_fd, -1, SEEK_CUR);
	} else {
		last_status = status;
		*ev_size = midi_event_size(status) + 1;
		last_size = *ev_size;
	}

	buf[0] = (uint8_t)status;

	if (status == 0xFF) {
		*ev_size = 0;
		if (feof(_fd))
			throw PrematureEOF();
		uint8_t type = fgetc(_fd);
		const uint32_t size = read_var_len(_fd);
		/*cerr.flags(ios::hex);
		cerr << "SMF - meta 0x" << (int)type << ", size = ";
		cerr.flags(ios::dec);
		cerr << size << endl;*/

		if ((uint8_t)type == 0x2F) {
			return -1; // we hit the logical EOF anyway...
		} else {
			fseek(_fd, size, SEEK_CUR);
			return 0;
		}
	}

	if (*ev_size > buf_len || *ev_size == 0 || feof(_fd)) {
		//cerr << "SMF - Skipping event" << endl;
		// Skip event, return 0
		fseek(_fd, *ev_size - 1, SEEK_CUR);
		return 0;
	} else {
		// Read event, return size
		if (ferror(_fd))
			throw CorruptFile();
		
		fread(buf+1, 1, *ev_size - 1, _fd);
	
		if ((buf[0] & 0xF0) == 0x90 && buf[2] == 0) {
			buf[0] = (0x80 | (buf[0] & 0x0F));
			buf[2] = 0x40;
		}
		
		return *ev_size;
	}
}


void
SMFReader::close()
{
	if (_fd)
		fclose(_fd);

	_fd = NULL;
}


uint32_t
SMFReader::read_var_len(FILE* fd) throw (PrematureEOF)
{
	if (feof(fd))
		throw PrematureEOF();

	uint32_t value;
	uint8_t  c;

	if ( (value = getc(fd)) & 0x80 ) {
		value &= 0x7F;
		do {
			if (feof(fd))
				throw PrematureEOF();
			value = (value << 7) + ((c = getc(fd)) & 0x7F);
		} while (c & 0x80);
	}

	return value;
}


} // namespace Evoral

