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

#include "pbd/debug.h"
#include "pbd/compose.h"

#include "memory.h"
#include "waves_midi_event.h"

using namespace ARDOUR;
using namespace PBD;

WavesMidiEvent::WavesMidiEvent (PmTimestamp timestamp)
    : _size (0)
    , _timestamp (timestamp)
    , _data (NULL)
    , _state (INCOMPLETE) 
{

}


WavesMidiEvent::WavesMidiEvent (PmTimestamp timestamp, const uint8_t* data, size_t datalen)
    : _size (datalen)
    , _timestamp (timestamp)
    , _data (data && datalen ? new uint8_t[ (datalen < sizeof (PmMessage)) ? sizeof (PmMessage) : datalen] : NULL)
    , _state (data && datalen ? COMPLETE : BROKEN) 
{
        DEBUG_TRACE (DEBUG::WavesMIDI, string_compose ( "WavesMidiEvent::WavesMidiEvent (const WavesMidiEvent& source) : Size=%1---%2\n", _size, datalen));
        if (_state == COMPLETE) {
                DEBUG_TRACE (DEBUG::WavesMIDI, string_compose ( "\t\t\t Allocated Size=%1\n", ((datalen < sizeof (PmMessage)) ? sizeof (PmMessage) : datalen)));
                memcpy (_data, data, datalen);

#ifndef NDEBUG
                if (DEBUG_ENABLED (DEBUG::WavesMIDI)) {
                        DEBUG_STR_DECL(a);
                        for (size_t i=0; i < datalen; ++i) {
                                DEBUG_STR_APPEND(a,std::hex);
                                DEBUG_STR_APPEND(a,"0x");
                                DEBUG_STR_APPEND(a,(int)data[i]);
                                DEBUG_STR_APPEND(a,' ');
                        }
                        DEBUG_STR_APPEND(a,'\n');
                        DEBUG_TRACE (DEBUG::WavesMIDI, DEBUG_STR(a).str());
	}
#endif

        }
}

WavesMidiEvent::WavesMidiEvent (const WavesMidiEvent& source)
    : _size (source.size ())
    , _timestamp (source.timestamp ())
    , _data ((source.size () && source.const_data ()) ? new uint8_t[ (source.size () < sizeof (PmMessage)) ? sizeof (PmMessage) : source.size ()] : NULL)
    , _state (source.state () ) 
{
        DEBUG_TRACE (DEBUG::WavesMIDI, string_compose ( "WavesMidiEvent::WavesMidiEvent (const WavesMidiEvent& source) : Size=%1---%2\n", _size, source.size ()));
        DEBUG_TRACE (DEBUG::WavesMIDI, string_compose ( "\t\t\t Allocated Size=%1\n", ((source.size () < sizeof (PmMessage)) ? sizeof (PmMessage) : source.size ())));
        if (_data && source.const_data ()) {
                memcpy (_data, source.const_data (), source.size ());

#ifndef NDEBUG
                if (DEBUG_ENABLED (DEBUG::WavesMIDI)) {
                        DEBUG_STR_DECL(a);
                        for (size_t i=0; i < source.size(); ++i) {
                                DEBUG_STR_APPEND(a,std::hex);
                                DEBUG_STR_APPEND(a,"0x");
                                DEBUG_STR_APPEND(a,(int)source.const_data()[i]);
                                DEBUG_STR_APPEND(a,' ');
                        }
                        DEBUG_STR_APPEND(a,'\n');
                        DEBUG_TRACE (DEBUG::WavesMIDI, DEBUG_STR(a).str());
                }
#endif
        }
}


WavesMidiEvent::~WavesMidiEvent ()
{
    delete _data;
}


WavesMidiEvent *WavesMidiEvent::append_data (const PmEvent &midi_event)
{
        switch ( _state ) {
        case INCOMPLETE: 
                break;
        default:
                DEBUG_TRACE (DEBUG::WavesMIDI, "WavesMidiEvent::append_data (): NO case INCOMPLETE\n");
                _state = BROKEN;
                return NULL;
        }
        
        size_t message_size = _midi_message_size (midi_event.message);
        uint8_t message_status = Pm_MessageStatus (midi_event.message);
        
        if (_data == NULL) { // This is a first event to add
                bool sysex = (message_status == SYSEX);
                _data = new unsigned char [sysex ? PM_DEFAULT_SYSEX_BUFFER_SIZE : sizeof (PmMessage)];
                if (!sysex)
                {
                        DEBUG_TRACE (DEBUG::WavesMIDI, "WavesMidiEvent::append_data (): SHORT MSG\n");
                        * (PmMessage*)_data = 0; 
                        switch (message_size) {
                        case 1:
                        case 2:
                        case 3:
                                _size = message_size;
                                DEBUG_TRACE (DEBUG::WavesMIDI, string_compose ( "WavesMidiEvent::append_data (): size = %1\n", _size));
                                break;
                         default:
                                DEBUG_TRACE (DEBUG::WavesMIDI, string_compose ( "WavesMidiEvent::append_data (): WRONG MESSAGE SIZE (%1 not %2) %3 [%4 %5 %6 %7] %8\n", 
                                                                                message_size,
                                                                                std::hex,
                                                                                (int) ((unsigned char*)&midi_event)[0],
                                                                                (int) ((unsigned char*)&midi_event)[1],
                                                                                (int) ((unsigned char*)&midi_event)[2],
                                                                                (int) ((unsigned char*)&midi_event)[3],
                                                                                std::dec));
                                _state = BROKEN;
                                return NULL;
                        }
                        memcpy (_data, &midi_event.message, _size);
                        _state = COMPLETE;
                        DEBUG_TRACE (DEBUG::WavesMIDI, string_compose ( "\t\t\t size = %1\n", _size));
                        return NULL;
                }
        }
        
        // Now let's parse to sysex msg
        if (message_status >= REAL_TIME_FIRST) { // Nested Real Time MIDI event
                WavesMidiEvent *waves_midi_message = new WavesMidiEvent (midi_event.timestamp);
                waves_midi_message->append_data (midi_event);
                return waves_midi_message;
        }
        
        if (message_status >= STATUS_FIRST && (message_status != EOX) && _size) { // Certainly it's a broken SYSEX case
                WavesMidiEvent *waves_midi_message = new WavesMidiEvent (midi_event.timestamp);
                waves_midi_message->append_data (midi_event);
                return waves_midi_message;
        }

        const uint8_t* source_data ((uint8_t*)&midi_event.message);
        
        for (size_t i = 0; i < sizeof (midi_event.message); ++i) {
                _data[_size] = source_data[i];
                _size++;
                
                if (source_data[i] == EOX) { // Ended SYSEX message
                        _state = COMPLETE;
                        return NULL;
                }
        }
        return NULL;
}

size_t WavesMidiEvent::_midi_message_size (PmMessage midi_message)
{
    static int high_lengths[] = {
        1, 1, 1, 1, 1, 1, 1, 1,         /* 0x00 through 0x70 */
        3, 3, 3, 3, 2, 2, 3, 1          /* 0x80 through 0xf0 */
    };

    static int low_lengths[] = {
        1, 2, 3, 2, 1, 1, 1, 1,         /* 0xf0 through 0xf7 */
        1, 1, 1, 1, 1, 1, 1, 1          /* 0xf8 through 0xff */
    };

    int midi_message_status = Pm_MessageStatus (midi_message);

    if (midi_message_status < STATUS_FIRST) {
        return sizeof (midi_message);
    }

    int high = midi_message_status >> 4;
    int low = midi_message_status & 0xF;

    return (high != 0xF) ? high_lengths[high] : low_lengths[low];
}
