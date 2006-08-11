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
#include <jack/jack.h>
#include <ardour/port.h>
#include <ardour/buffer.h>

namespace ARDOUR {

class AudioEngine;

class AudioPort : public Port {
   public:
	virtual ~AudioPort() { 
		free (_port);
	}

	void cycle_start(jack_nframes_t nframes);
	void cycle_end();

	DataType type() const { return DataType(DataType::AUDIO); }

	Buffer& get_buffer () {
		return _buffer;
	}
	
	AudioBuffer& get_audio_buffer() {
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

	float                       peak_db() const { return _peak_db; }
	jack_default_audio_sample_t peak()    const { return _peak; }

	uint32_t short_overs () const { return _short_overs; }
	uint32_t long_overs ()  const { return _long_overs; }
	
	static void set_short_over_length (jack_nframes_t);
	static void set_long_over_length (jack_nframes_t);

	/** Assumes that the port is an audio output port */
	void silence (jack_nframes_t nframes, jack_nframes_t offset) {
		if (!_silent) {
			_buffer.clear(offset);
			if (offset == 0 && nframes == _buffer.capacity()) {
				_silent = true;
			}
		}
	}
	
  protected:
	friend class AudioEngine;

	AudioPort (jack_port_t *port);
	void reset ();
	
	/* engine isn't supposed to access below here */

	AudioBuffer _buffer;

	jack_nframes_t               _overlen;
	jack_default_audio_sample_t  _peak;
	float                        _peak_db;
	uint32_t                     _short_overs;
	uint32_t                     _long_overs;
	
	static jack_nframes_t        _long_over_length;
	static jack_nframes_t        _short_over_length;
};
 
} // namespace ARDOUR

#endif /* __ardour_audio_port_h__ */
