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

#include "waves_midiport.h"
#include "waves_midi_event.h"

using namespace ARDOUR;

WavesMidiPort::WavesMidiPort (const std::string& port_name, PortFlags flags)
    : WavesDataPort (port_name, flags)
    , _midi_device (NULL)
    , _waves_midi_buffer (port_name)
{       
}

struct MidiEventSorter {
	bool operator() (const WavesMidiEvent* a, const WavesMidiEvent* b) {
		return *a < *b;
	}
};

void* 
WavesMidiPort::get_buffer (pframes_t nframes)
{
    if (is_input ()) {
		std::vector<WavesDataPort*>::const_iterator cit = get_connections ().begin ();
        if (cit != get_connections ().end ()) {
			_waves_midi_buffer.clear ();
			WavesMidiBuffer& target = _waves_midi_buffer;

			do	{
				/* In fact, the static casting to (const WavesMidiPort*) is not that safe.
				 * However, mixing the buffers is assumed in the time critical conditions.
				 * Base class WavesDataPort is supposed to provide enough consistentcy
				 * of the connections.
				 */
				target += ((const WavesMidiPort*)*cit)->const_buffer ();
			}while((++cit) != get_connections ().end ());

			std::sort (target.begin (), target.end (), MidiEventSorter());
		}
	}

    return &_waves_midi_buffer;
}

void
WavesMidiPort::_wipe_buffer()
{
	_waves_midi_buffer.clear ();
}
