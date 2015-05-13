/*
    Copyright (C) 2013 Waves Audio Ltd.

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

*/

#ifndef __libardour_waves_audioport_h__
#define __libardour_waves_audioport_h__

#include "memory.h"
#include "waves_dataport.h"
        
namespace ARDOUR {

class WavesAudioPort : public WavesDataPort {

public:
    enum BufferSize {
        MAX_BUFFER_SIZE_SAMPLES = 8192,
        MAX_BUFFER_SIZE_BYTES = sizeof (Sample) * MAX_BUFFER_SIZE_SAMPLES
    };

    WavesAudioPort (const std::string& port_name, PortFlags flags);

    virtual ~WavesAudioPort ();

    virtual DataType type () const {    return DataType::AUDIO; };

    inline Sample* buffer () { return _buffer; }
    inline const Sample* const_buffer () const { return _buffer; }

    virtual void* get_buffer (pframes_t nframes);

protected:
	virtual void _wipe_buffer();

private:

    Sample *_buffer;
};

} // namespace

#endif /* __libardour_waves_audioport_h__ */
    
