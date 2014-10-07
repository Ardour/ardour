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

#ifndef __libardour_waves_midi_event_h__
#define __libardour_waves_midi_event_h__

#include <stdlib.h>
#include <portmidi/portmidi.h>
#include "ardour/types.h"

namespace ARDOUR {

class WavesMidiEvent
{
public:
    enum State {
        INCOMPLETE,
        BROKEN,
        COMPLETE
    };

    WavesMidiEvent (PmTimestamp timestamp);
    WavesMidiEvent (PmTimestamp timestamp, const uint8_t* data, size_t datalen);
    WavesMidiEvent (const WavesMidiEvent& source);
    ~WavesMidiEvent ();
    
    WavesMidiEvent *append_data (const PmEvent &midi_event);

    inline State state () const { return _state; };
    inline size_t size () const { return _size; };
    inline PmTimestamp timestamp () const { return _timestamp; };
    inline void set_timestamp (PmTimestamp time_stamp) { _timestamp = time_stamp; };
    inline const unsigned char* const_data () const { return _data; };
    inline unsigned char* data () { return _data; };
    inline bool operator< (const WavesMidiEvent &other) const { return timestamp () < other.timestamp (); };
    inline bool sysex () const { return _data && (*_data == SYSEX); };

private:

    enum
    {
        SYSEX = 0xF0,
        EOX = 0xF7,
        REAL_TIME_FIRST = 0xF8,
        STATUS_FIRST = 0x80
    };

    size_t _size;
    PmTimestamp _timestamp;
    uint8_t *_data;
    State _state;

    static size_t _midi_message_size (PmMessage midi_message);
};


} // namespace

#endif /* __libardour_waves_midi_event_h__ */
