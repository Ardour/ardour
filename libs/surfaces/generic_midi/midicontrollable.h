/*
    Copyright (C) 1998-2006 Paul Davis
 
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

#ifndef __gm_midicontrollable_h__
#define __gm_midicontrollable_h__

#include <string>

#include "midi++/types.h"

#include "pbd/controllable.h"
#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/types.h"

namespace PBD {
	class ControllableDescriptor;
}

namespace MIDI {
	class Channel;
	class Parser;
}

class GenericMidiControlProtocol;

namespace ARDOUR {
	class AsyncMIDIPort;
}

class MIDIControllable : public PBD::Stateful
{
  public:
        MIDIControllable (GenericMidiControlProtocol *, MIDI::Parser&, PBD::Controllable&, bool momentary);
        MIDIControllable (GenericMidiControlProtocol *, MIDI::Parser&, bool momentary = false);
	virtual ~MIDIControllable ();

	int init (const std::string&);

	void rediscover_controllable ();
	bool bank_relative() const { return _bank_relative; }
	uint32_t rid() const { return _rid; }
	std::string what() const { return _what; }

	MIDI::byte* write_feedback (MIDI::byte* buf, int32_t& bufsize, bool force = false);
	
	void midi_rebind (MIDI::channel_t channel=-1);
	void midi_forget ();
	void learn_about_external_control ();
	void stop_learning ();
	void drop_external_control ();

	bool get_midi_feedback () { return feedback; }
	void set_midi_feedback (bool val) { feedback = val; }

	int control_to_midi(float val);
	float midi_to_control(int val);

	bool learned() const { return _learned; }

        MIDI::Parser& get_parser() { return _parser; }
	PBD::Controllable* get_controllable() const { return controllable; }
	void set_controllable (PBD::Controllable*);
	const std::string& current_uri() const { return _current_uri; }

	PBD::ControllableDescriptor& descriptor() const { return *_descriptor; }

	std::string control_description() const { return _control_description; }

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void bind_midi (MIDI::channel_t, MIDI::eventType, MIDI::byte);
	MIDI::channel_t get_control_channel () { return control_channel; }
	MIDI::eventType get_control_type () { return control_type; }
	MIDI::byte get_control_additional () { return control_additional; }

        int lookup_controllable();
	
  private:

	int max_value_for_type () const;

	GenericMidiControlProtocol* _surface;
	PBD::Controllable* controllable;
	PBD::ControllableDescriptor* _descriptor;
	std::string     _current_uri;
        MIDI::Parser&   _parser;
	bool             setting;
	int              last_value;
	float            last_controllable_value;
	bool            _momentary;
	bool            _is_gain_controller;
	bool            _learned;
	int              midi_msg_id;      /* controller ID or note number */
	PBD::ScopedConnection midi_sense_connection[2];
	PBD::ScopedConnection midi_learn_connection;
        PBD::ScopedConnection controllable_death_connection;
	/** the type of MIDI message that is used for this control */
	MIDI::eventType  control_type;
	MIDI::byte       control_additional;
	MIDI::channel_t  control_channel;
	std::string     _control_description;
	bool             feedback;
	uint32_t        _rid;
	std::string     _what;
	bool            _bank_relative;

        void drop_controllable();

	void midi_receiver (MIDI::Parser &p, MIDI::byte *, size_t);
	void midi_sense_note (MIDI::Parser &, MIDI::EventTwoBytes *, bool is_on);
	void midi_sense_note_on (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_note_off (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_controller (MIDI::Parser &, MIDI::EventTwoBytes *);
	void midi_sense_program_change (MIDI::Parser &, MIDI::byte);
	void midi_sense_pitchbend (MIDI::Parser &, MIDI::pitchbend_t);
};

#endif // __gm_midicontrollable_h__

