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

#ifndef EVORAL_LIB_SMF_HPP
#define EVORAL_LIB_SMF_HPP

#include "evoral/StandardMIDIFile.hpp"

#include <smf.h>
#include <cassert>

namespace Evoral {
	
template<typename Time> class Event;
template<typename Time> class EventRingBuffer;

/** Standard Midi File (Type 0)
 */
template<typename Time>
class LibSMF : public StandardMIDIFile<Time> {
public:
	LibSMF() : _last_ev_time(0), _smf(0), _smf_track(0), _empty(true) {};
	virtual ~LibSMF() {	
		if (_smf) {
			smf_delete(_smf);
			_smf = 0;
			_smf_track = 0;
		} 
	}

	void seek_to_start() const;
	
	uint16_t ppqn()     const { return _ppqn; }
	bool     is_empty() const { return _empty; }
	bool     eof()      const { assert(false); return true; }
	
	Time last_event_time() const { return _last_ev_time; }
	
	void begin_write(FrameTime start_time);
	void append_event_unlocked(uint32_t delta_t, const Event<Time>& ev);
	void end_write();
	
	void flush() {};
	int  flush_header() { return 0; }
	int  flush_footer() { return 0; }

protected:
	int  open(const std::string& path);
	void close();
	
	int read_event(uint32_t* delta_t, uint32_t* size, uint8_t** buf) const;

private:
	static const uint16_t _ppqn = 19200;
	
	Time     _last_ev_time; ///< last frame time written, relative to source start
	
	std::string  _path;
	smf_t*       _smf;
	smf_track_t* _smf_track;

	bool         _empty; ///< true iff file contains(non-empty) events
};

}; /* namespace Evoral */

#endif /* EVORAL_LIB_SMF_HPP */

