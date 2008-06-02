/*
    Copyright (C) 2007 Paul Davis 

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

#ifndef __ardour_internal_audio_port_h__
#define __ardour_internal_audio_port_h__

#include <sigc++/signal.h>
#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>
#include <ardour/internal_port.h>
#include <ardour/audio_port.h>

namespace ARDOUR {

class AudioEngine;
class InternalAudioPort : public AudioPort, public InternalPort {
   public:
	void cycle_start(nframes_t nframes) {
		_buffer.silence (nframes);
	}

	AudioBuffer& get_audio_buffer();

	void set_mixdown_function (void (*func)(const std::list<InternalPort*>&, AudioBuffer&, nframes_t, nframes_t));
	void reset ();

  protected:
	friend class AudioEngine;

	InternalAudioPort (const std::string& name, Flags flags);
	void (*_mixdown)(const std::list<InternalPort*>&, AudioBuffer&, nframes_t, nframes_t);

	static void default_mixdown (const std::list<InternalPort*>&, AudioBuffer&, nframes_t, nframes_t);
};
 
} // namespace ARDOUR

#endif /* __ardour_internal_audio_port_h__ */
