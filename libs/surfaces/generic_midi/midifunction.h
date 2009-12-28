/*
    Copyright (C) 2009 Paul Davis
 
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

#ifndef __gm_midifunction_h__
#define __gm_midifunction_h__

#include <string>

#include "midi++/types.h"

#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/types.h"

namespace MIDI {
	class Channel;
	class Port;
	class Parser;
}

class BasicUI;

class MIDIFunction : public PBD::Stateful
{
  public:
	enum Function { 
		TransportRoll,
		TransportStop,
		TransportZero,
		TransportStart,
		TransportEnd
	};

	MIDIFunction (MIDI::Port&);
	virtual ~MIDIFunction ();

	int init (BasicUI&, const std::string&);

	MIDI::Port& get_port() const { return _port; }
	const std::string& function_name() const { return _function_name; }

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void bind_midi (MIDI::channel_t, MIDI::eventType, MIDI::byte);
	MIDI::channel_t get_control_channel () { return control_channel; }
	MIDI::eventType get_control_type () { return control_type; }
	MIDI::byte get_control_additional () { return control_additional; }
	
  private:
	Function        _function;
	BasicUI*        _ui;
	std::string     _function_name;
	MIDI::Port&     _port;
	PBD::ScopedConnection midi_sense_connection[2];
	MIDI::eventType  control_type;
	MIDI::byte       control_additional;
	MIDI::channel_t  control_channel;

	void init (const std::string& function_name);
	void execute ();

	void midi_sense_note (MIDI::Parser &, MIDI::EventTwoBytes *, bool is_on);
	void midi_sense_note_on (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_note_off (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_controller (MIDI::Parser &, MIDI::EventTwoBytes *);
	void midi_sense_program_change (MIDI::Parser &, MIDI::byte);
};

#endif // __gm_midicontrollable_h__

