/* This file is part of Evoral.
 * Copyright(C) 2008 David Robillard <http://drobilla.net>
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

#ifndef EVORAL_OLD_SMF_HPP
#define EVORAL_OLD_SMF_HPP

#include "evoral/visibility.h"

namespace Evoral {

template<typename Time> class Event;
template<typename Time> class EventRingBuffer;


/** Standard Midi File (Type 0)
 */
template<typename Time>
class LIBEVORAL_API SMF {
public:
	SMF();
	virtual ~SMF();

	void seek_to_start() const;

	uint16_t ppqn()     const { return _ppqn; }
	bool     is_empty() const { return _empty; }
	bool     eof()      const { return feof(_fd); }

	Time last_event_time() const { return _last_ev_time; }

	void begin_write();
	void append_event_delta(uint32_t delta_t, const Event<Time>& ev);
	void end_write() THROW_FILE_ERROR;

	void flush();
	int  flush_header();
	int  flush_footer();

protected:
	int  open(const std::string& path) THROW_FILE_ERROR;
	void close() THROW_FILE_ERROR;

	int read_event(uint32_t* delta_t, uint32_t* size, uint8_t** buf) const;

private:
	/** Used by flush_footer() to find the position to write the footer */
	void seek_to_footer_position();

	/** Write the track footer at the current seek position */
	void write_footer();

	void     write_chunk_header(const char id[4], uint32_t length);
	void     write_chunk(const char id[4], uint32_t length, void* data);
	size_t   write_var_len(uint32_t val);
	uint32_t read_var_len() const;

	static const uint16_t _ppqn = 19200;

	FILE*    _fd;
	Time     _last_ev_time; ///< last frame time written, relative to source start
	uint32_t _track_size;
	uint32_t _header_size; ///< size of SMF header, including MTrk chunk header
	bool     _empty; ///< true iff file contains(non-empty) events
};

}; /* namespace Evoral */

#endif /* EVORAL_OLD_SMF_HPP */

