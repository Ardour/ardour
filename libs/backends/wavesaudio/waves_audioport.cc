/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "waves_audioport.h"
#include "ardour/runtime_functions.h"

using namespace ARDOUR;

WavesAudioPort::WavesAudioPort (const std::string& port_name, PortFlags flags)
    : WavesDataPort (port_name, flags)    
{
    memset (_buffer, 0, sizeof (_buffer));
}


void* WavesAudioPort::get_buffer (pframes_t nframes)
{
    if (is_input ()) {
        
        std::vector<WavesDataPort*>::const_iterator it = get_connections ().begin ();
        
        if (it != get_connections ().end ()) {
            /* In fact, the static casting to (const WavesAudioPort*) is not that safe.
             * However, mixing the buffers is assumed in the time critical conditions.
             * Base class WavesDataPort takes is supposed to provide enough consistentcy
             * of the connections.
             */

			// get first buffer data
			memcpy (_buffer, ((const WavesAudioPort*)*it)->const_buffer (), nframes * sizeof (Sample));
			++it;
            
			// mix the rest
			for (; it != get_connections ().end (); ++it) {
                Sample* tgt = buffer ();
                const Sample* src = ((const WavesAudioPort*)*it)->const_buffer ();

				// use otimized function to mix the buffers
				ARDOUR::mix_buffers_no_gain (tgt, src, nframes);
            }
        }
    }
    return _buffer;
}


void
WavesAudioPort::_wipe_buffer()
{
	memset (_buffer, 0, sizeof (_buffer));
}