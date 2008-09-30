/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
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

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <cassert>
#include <iostream>
#include <glibmm/miscutils.h>
#include <evoral/midi_util.h>
#include <evoral/SMF.hpp>
#include <evoral/SMFReader.hpp>
#include <evoral/Event.hpp>

using namespace std;

namespace Evoral {

SMF::SMF()
	: _fd(0)
	, _last_ev_time(0)
	, _track_size(4) // 4 bytes for the ever-present EOT event
	, _header_size(22)
	, _empty(true)
{
}

SMF::~SMF()
{
}

/** Attempt to open the SMF file for reading and writing.
 *
 * Currently SMF is always read/write.
 *
 * \return  0 on success
 *         -1 if the file can not be opened for reading,
 *         -2 if the file can not be opened for writing
 */
int
SMF::open(const std::string& path)
{
	//cerr << "Opening SMF file " << path() << " writeable: " << writable() << endl;
	_fd = fopen(path.c_str(), "r+");

	// File already exists
	if (_fd) {
		fseek(_fd, _header_size - 4, 0);
		uint32_t track_size_be = 0;
		fread(&track_size_be, 4, 1, _fd);
		_track_size = GUINT32_FROM_BE(track_size_be);
		_empty = _track_size > 4;
		//cerr << "SMF - read track size " << _track_size << endl;

	// We're making a new file
	} else {
		_fd = fopen(path.c_str(), "w+");
		if (_fd == NULL) {
			cerr << "ERROR: Can not open SMF file " << path << " for writing: " <<
				strerror(errno) << endl;
			return -2;
		}
		_track_size = 4;
		_empty = true;

		// Write a tentative header just to pad things out so writing happens in the right spot
		flush_header();
		flush_footer();
	}
		
	return (_fd == 0) ? -1 : 0;
}

void
SMF::close()
{
	if (_fd) {
		flush_header();
		flush_footer();
		fclose(_fd);
		_fd = NULL;
	}
}

void
SMF::seek_to_start() const
{
	fseek(_fd, _header_size, SEEK_SET);
}

void
SMF::seek_to_footer_position()
{
	uint8_t buffer[4];
	
	// Check if there is a track end marker at the end of the data
	fseek(_fd, -4, SEEK_END);
	size_t read_bytes = fread(buffer, sizeof(uint8_t), 4, _fd);

	if ((read_bytes == 4)
			&& buffer[0] == 0x00
			&& buffer[1] == 0xFF
			&& buffer[2] == 0x2F
			&& buffer[3] == 0x00) {
		// there is one, so overwrite it
		fseek(_fd, -4, SEEK_END);
	} else {
		// there is none, so append
		fseek(_fd, 0, SEEK_END);
	}
}

void
SMF::flush()
{
	fflush(_fd);
}

int
SMF::flush_header()
{
	// FIXME: write timeline position somehow?
	
	//cerr << path() << " SMF Flushing header\n";

	assert(_fd);

	const uint16_t type     = GUINT16_TO_BE(0);     // SMF Type 0 (single track)
	const uint16_t ntracks  = GUINT16_TO_BE(1);     // Number of tracks (always 1 for Type 0)
	const uint16_t division = GUINT16_TO_BE(_ppqn); // Pulses per quarter note (beat)

	char data[6];
	memcpy(data, &type, 2);
	memcpy(data+2, &ntracks, 2);
	memcpy(data+4, &division, 2);

	//_fd = freopen(path().c_str(), "r+", _fd);
	//assert(_fd);
	fseek(_fd, 0, SEEK_SET);
	write_chunk("MThd", 6, data);
	write_chunk_header("MTrk", _track_size); 

	fflush(_fd);

	return 0;
}

int
SMF::flush_footer()
{
	//cerr << path() << " SMF Flushing footer\n";
	seek_to_footer_position();
	write_footer();
	seek_to_footer_position();

	return 0;
}

void
SMF::write_footer()
{
	write_var_len(0);
	char eot[3] = { 0xFF, 0x2F, 0x00 }; // end-of-track meta-event
	fwrite(eot, 1, 3, _fd);
	fflush(_fd);
}

/** Returns the offset of the first event in the file with a time past @a start,
 * relative to the start of the source.
 *
 * Returns -1 if not found.
 */
/*
long
SMF::find_first_event_after(nframes_t start)
{
	// FIXME: obviously this is slooow
	
	fseek(_fd, _header_size, 0);

	while ( ! feof(_fd) ) {
		const uint32_t delta_time = read_var_len();

		if (delta_time > start)
			return delta_time;
	}

	return -1;
}
*/

/** Read an event from the current position in file.
 *
 * File position MUST be at the beginning of a delta time, or this will die very messily.
 * ev.buffer must be of size ev.size, and large enough for the event.  The returned event
 * will have it's time field set to it's delta time, in SMF tempo-based ticks, using the
 * rate given by ppqn() (it is the caller's responsibility to calculate a real time).
 *
 * \a size should be the capacity of \a buf.  If it is not large enough, \a buf will
 * be freed and a new buffer allocated in its place, the size of which will be placed
 * in size.
 *
 * Returns event length (including status byte) on success, 0 if event was
 * skipped (eg a meta event), or -1 on EOF (or end of track).
 */
int
SMF::read_event(uint32_t* delta_t, uint32_t* size, uint8_t** buf) const
{
	if (feof(_fd)) {
		return -1;
	}

	assert(delta_t);
	assert(size);
	assert(buf);

	try {
		*delta_t = SMFReader::read_var_len(_fd);
	} catch (...) {
		return -1; // Premature EOF
	}
	
	if (feof(_fd)) {
		return -1; // Premature EOF
	}

	const int status = fgetc(_fd);

	if (status == EOF) {
		return -1; // Premature EOF
	}

	//printf("Status @ %X = %X\n", (unsigned)ftell(_fd) - 1, status);

	if (status == 0xFF) {
		if (feof(_fd)) {
			return -1; // Premature EOF
		}
		const int type = fgetc(_fd);
		if ((unsigned char)type == 0x2F) {
			return -1; // hit end of track
		} else {
			*size = 0;
			return 0;
		}
	}
	
	const int event_size = midi_event_size((unsigned char)status) + 1;
	if (event_size <= 0) {
		*size = 0;
		return 0;
	}
	
	// Make sure we have enough scratch buffer
	if (*size < (unsigned)event_size)
		*buf = (uint8_t*)realloc(*buf, event_size);
	
	*size = event_size;

	(*buf)[0] = (unsigned char)status;
	if (event_size > 1)
		fread((*buf) + 1, 1, *size - 1, _fd);

	/*printf("SMF %s read event: delta = %u, size = %u, data = ", _name.c_str(), *delta_t, *size);
	for (size_t i=0; i < *size; ++i) {
		printf("%X ", (*buf)[i]);
	}
	printf("\n");*/
	
	return (int)*size;
}

void
SMF::append_event_unlocked(uint32_t delta_t, const Evoral::Event& ev)
{
	if (ev.size() == 0)
		return;

	const size_t stamp_size = write_var_len(delta_t);
	fwrite(ev.buffer(), 1, ev.size(), _fd);

	_track_size += stamp_size + ev.size();
	_last_ev_time = ev.time();

	if (ev.size() > 0)
		_empty = false;
}

void
SMF::begin_write(nframes_t start_frame)
{
	_last_ev_time = 0;
	fseek(_fd, _header_size, SEEK_SET);
}

void
SMF::end_write()
{
	flush_header();
	flush_footer();
}

void
SMF::write_chunk_header(const char id[4], uint32_t length)
{
	const uint32_t length_be = GUINT32_TO_BE(length);

	fwrite(id, 1, 4, _fd);
	fwrite(&length_be, 4, 1, _fd);
}

void
SMF::write_chunk(const char id[4], uint32_t length, void* data)
{
	write_chunk_header(id, length);
	
	fwrite(data, 1, length, _fd);
}

/** Returns the size (in bytes) of the value written. */
size_t
SMF::write_var_len(uint32_t value)
{
	size_t ret = 0;

	uint32_t buffer = value & 0x7F;

	while ( (value >>= 7) ) {
		buffer <<= 8;
		buffer |= ((value & 0x7F) | 0x80);
	}

	while (true) {
		//printf("Writing var len byte %X\n", (unsigned char)buffer);
		++ret;
		fputc(buffer, _fd);
		if (buffer & 0x80)
			buffer >>= 8;
		else
			break;
	}

	return ret;
}

} // namespace Evoral
