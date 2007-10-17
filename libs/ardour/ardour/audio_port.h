/*
    Copyright (C) 2002 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: port.h 712 2006-07-28 01:08:57Z drobilla $
*/

#ifndef __ardour_audio_port_h__
#define __ardour_audio_port_h__

#include <sigc++/signal.h>
#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>
#include <ardour/port.h>
#include <ardour/audio_buffer.h>

namespace ARDOUR {

class AudioEngine;

class AudioPort : public virtual Port {
   public:
	DataType type() const { return DataType::AUDIO; }

	virtual Buffer& get_buffer () {
		return _buffer;
	}
	
	virtual AudioBuffer& get_audio_buffer() {
		return _buffer;
	}

	void reset_overs () {
		_short_overs = 0;
		_long_overs = 0;
		_overlen = 0;
	}

	void reset_peak_meter () {
		_peak = 0;
	}
	
	void reset_meters () {
		reset_peak_meter ();
		reset_overs ();
	}

	float  peak_db() const { return _peak_db; }
	Sample peak()    const { return _peak; }

	uint32_t short_overs () const { return _short_overs; }
	uint32_t long_overs ()  const { return _long_overs; }
	
	static void set_short_over_length (nframes_t);
	static void set_long_over_length (nframes_t);

  protected:
	friend class AudioEngine;

	AudioPort ();          // data buffer comes from elsewhere (e.g. JACK)
	AudioPort (nframes_t); // data buffer owned by ardour
	void reset ();
	
	/* engine isn't supposed to access below here */

	AudioBuffer _buffer;

	nframes_t _overlen;
	Sample    _peak;
	float     _peak_db;
	uint32_t  _short_overs;
	uint32_t  _long_overs;
	
	static nframes_t _long_over_length;
	static nframes_t _short_over_length;
};
 
} // namespace ARDOUR

#endif /* __ardour_audio_port_h__ */
