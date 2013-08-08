/*
    Copyright (C) 2002-2009 Paul Davis

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

#include "ardour/port.h"
#include "ardour/audio_buffer.h"

namespace ARDOUR {

class AudioPort : public Port
{
   public:
	~AudioPort ();

	DataType type () const {
		return DataType::AUDIO;
	}

	void cycle_start (pframes_t);
	void cycle_end (pframes_t);
	void cycle_split ();

	Buffer& get_buffer (pframes_t nframes) {
		return get_audio_buffer (nframes);
	}

	AudioBuffer& get_audio_buffer (pframes_t nframes);

  protected:
	friend class PortManager;
	AudioPort (std::string const &, PortFlags);

        /* special access for PortManager only (hah, C++) */
        Sample* engine_get_whole_audio_buffer ();

  private:
	AudioBuffer* _buffer;
        bool         _buf_valid; 
};

} // namespace ARDOUR

#endif /* __ardour_audio_port_h__ */
