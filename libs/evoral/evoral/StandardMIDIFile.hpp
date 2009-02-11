/* This file is part of Evoral.
 * Copyright(C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright(C) 2000-2008 Paul Davis 
 * Author: Hans Baier
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

#ifndef EVORAL_STANDARD_MIDI_FILE_HPP
#define EVORAL_STANDARD_MIDI_FILE_HPP

#include <string>
#include "evoral/types.hpp"

namespace Evoral {
	
template<typename Time> class Event;
template<typename Time> class EventRingBuffer;

#define THROW_FILE_ERROR throw(typename StandardMIDIFile<Time>::FileError)

/** Standard MIDI File interface
 */
template<typename Time>
class StandardMIDIFile {
public:
	class FileError : public std::exception {
		const char* what() const throw() { return "libsmf error"; }
	};

	virtual void seek_to_start() const = 0;
	
	virtual uint16_t ppqn()     const = 0;
	virtual bool     is_empty() const = 0;
	virtual bool     eof()      const = 0;
	
	virtual Time last_event_time() const = 0;
	
	virtual void begin_write(FrameTime start_time) = 0;
	virtual void append_event_unlocked(uint32_t delta_t, const Event<Time>& ev) = 0;
	virtual void end_write() throw(FileError) = 0;
	
	virtual void flush() = 0;
	virtual int  flush_header() = 0;
	virtual int  flush_footer() = 0;

protected:
	virtual int  open(const std::string& path) throw(FileError) = 0;
	virtual void close() throw(FileError) = 0;
	
	virtual int read_event(uint32_t* delta_t, uint32_t* size, uint8_t** buf) const = 0;

};

}; /* namespace Evoral */

#endif /* EVORAL_STANDARD_MIDI_FILE_HPP */

