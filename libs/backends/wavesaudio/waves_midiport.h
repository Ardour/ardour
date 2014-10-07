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

#ifndef __libardour_waves_midiport_h__
#define __libardour_waves_midiport_h__

#include "waves_dataport.h"
#include "waves_midi_buffer.h"

namespace ARDOUR {

class WavesMidiEvent;
class WavesMidiDevice;
class WavesMidiEvent;

class WavesMidiPort : public WavesDataPort {
public:
    enum BufferSize {
        // This value has nothing to do with reality as buffer of MIDI Port is not a flat array.
        // It's an iterated list.
        MAX_BUFFER_SIZE_BYTES = 8192
    };

    WavesMidiPort (const std::string& port_name, PortFlags flags);
    virtual ~WavesMidiPort (){};

    virtual DataType type () const {    return DataType::MIDI; };

    virtual void* get_buffer (pframes_t nframes);

    inline WavesMidiBuffer& buffer () { return _waves_midi_buffer; }
    inline const WavesMidiBuffer& const_buffer () const { return _waves_midi_buffer; }

    inline void set_midi_device (WavesMidiDevice* midi_device) { _midi_device = midi_device; };
    inline WavesMidiDevice* midi_device () const { return _midi_device; };

protected:
	virtual void _wipe_buffer();

private:
    WavesMidiDevice * _midi_device;
    WavesMidiBuffer _waves_midi_buffer;
};

} // namespace

#endif /* __libardour_waves_midiport_h__ */
    
