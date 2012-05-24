/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_MIDI_EVENT_HPP
#define EVORAL_MIDI_EVENT_HPP

#include <cmath>
#include <boost/shared_ptr.hpp>
#include "evoral/Event.hpp"
#include "evoral/midi_events.h"
#ifdef EVORAL_MIDI_XML
class XMLNode;
#endif

namespace Evoral {

/** MIDI helper functions for an Event.
 *
 * This class contains no data, an Evoral::Event can be cast to a MIDIEvent
 * but the application must make sure the Event actually contains
 * valid MIDI data for these functions to make sense.
 */
template<typename Time>
class MIDIEvent : public Event<Time> {
public:
	MIDIEvent(EventType type=0, Time time=0, uint32_t size=0, uint8_t* buf=NULL, bool alloc=false)
		: Event<Time>(type, time, size, buf, alloc)
	{}

	MIDIEvent(const Event<Time>& copy, bool alloc)
		: Event<Time>(copy, alloc)
	{}

#ifdef EVORAL_MIDI_XML
	/** Event from XML ala http://www.midi.org/dtds/MIDIEvents10.dtd */
	MIDIEvent(const XMLNode& event);

	/** Event to XML ala http://www.midi.org/dtds/MIDIEvents10.dtd */
	boost::shared_ptr<XMLNode> to_xml() const;
#endif

	inline uint8_t  type()                  const { return (this->_buf[0] & 0xF0); }
	inline void     set_type(uint8_t type)        { this->_buf[0] =   (0x0F & this->_buf[0])
	                                                                | (0xF0 & type); }
	inline uint8_t  channel()               const { return (this->_buf[0] & 0x0F); }
	inline void     set_channel(uint8_t channel)  { this->_buf[0] =   (0xF0 & this->_buf[0])
	                                                                | (0x0F & channel); }
	inline bool     is_note_on()            const { return (type() == MIDI_CMD_NOTE_ON); }
	inline bool     is_note_off()           const { return (type() == MIDI_CMD_NOTE_OFF); }
	inline bool     is_cc()                 const { return (type() == MIDI_CMD_CONTROL); }
	inline bool     is_pitch_bender()       const { return (type() == MIDI_CMD_BENDER); }
	inline bool     is_pgm_change()         const { return (type() == MIDI_CMD_PGM_CHANGE); }
	inline bool     is_note()               const { return (is_note_on() || is_note_off()); }
	inline bool     is_aftertouch()         const { return (type() == MIDI_CMD_NOTE_PRESSURE); }
	inline bool     is_channel_pressure()   const { return (type() == MIDI_CMD_CHANNEL_PRESSURE); }
	inline uint8_t  note()                  const { return (this->_buf[1]); }
	inline void     set_note(uint8_t n)           { this->_buf[1] = n; }
	inline uint8_t  velocity()              const { return (this->_buf[2]); }
	inline void     set_velocity(uint8_t value)   { this->_buf[2] = value; }
	inline void     scale_velocity(float factor)  {
		if (factor < 0) factor = 0;
		this->_buf[2] = (uint8_t) lrintf (this->_buf[2]*factor);
		if (this->_buf[2] > 127) this->_buf[2] = 127;
	}
	inline uint8_t  cc_number()             const { return (this->_buf[1]); }
	inline void     set_cc_number(uint8_t number) { this->_buf[1] = number; }
	inline uint8_t  cc_value()              const { return (this->_buf[2]); }
	inline void     set_cc_value(uint8_t value)   { this->_buf[2] = value; }
	inline uint8_t  pitch_bender_lsb()      const { return (this->_buf[1]); }
	inline uint8_t  pitch_bender_msb()      const { return (this->_buf[2]); }
	inline uint16_t pitch_bender_value()    const { return ( ((0x7F & this->_buf[2]) << 7)
	                                                        | (0x7F & this->_buf[1]) ); }
	inline uint8_t  pgm_number()            const { return (this->_buf[1]); }
	inline void     set_pgm_number(uint8_t number){ this->_buf[1] = number; }
	inline uint8_t  aftertouch()            const { return (this->_buf[1]); }
	inline uint8_t  channel_pressure()      const { return (this->_buf[1]); }
	inline bool     is_channel_event()      const { return (0x80 <= type()) && (type() <= 0xE0); }
	inline bool     is_smf_meta_event()     const { return this->_buf[0] == 0xFF; }
	inline bool     is_sysex()              const { return this->_buf[0] == 0xF0
	                                                    || this->_buf[0] == 0xF7; }
        inline bool     is_spp()                const { return this->_buf[0] == 0xF2 && this->size() == 1; }
        inline bool     is_mtc_quarter()        const { return this->_buf[0] == 0xF1 && this->size() == 1; }
        inline bool     is_mtc_full()           const { 
		return this->size() == 10 && this->_buf[0] == 0xf0 && this->_buf[1] == 0x7f && 
			this->_buf[3] == 0x01 && this->_buf[4] == 0x01;
	}
};

} // namespace Evoral

#endif // EVORAL_MIDI_EVENT_HPP
