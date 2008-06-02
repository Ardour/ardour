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

#include <sigc++/sigc++.h>

#include <midi++/types.h>
#include <pbd/controllable.h>
#include <pbd/stateful.h>
#include <ardour/types.h>

namespace MIDI {

class Channel;
class Port;
class Parser;

}

class MIDIControllable : public PBD::Stateful
{
  public:
	MIDIControllable (MIDI::Port&, PBD::Controllable&, bool bistate = false);
	virtual ~MIDIControllable ();

	void send_feedback ();
	MIDI::byte* write_feedback (MIDI::byte* buf, int32_t& bufsize, bool force = false);
	
	void midi_rebind (MIDI::channel_t channel=-1);
	void midi_forget ();
	void learn_about_external_control ();
	void stop_learning ();
	void drop_external_control ();

	bool get_midi_feedback () { return feedback; }
	void set_midi_feedback (bool val) { feedback = val; }

	MIDI::Port& get_port() const { return _port; }
	PBD::Controllable& get_controllable() const { return controllable; }

	std::string control_description() const { return _control_description; }

	XMLNode& get_state (void);
	int set_state (const XMLNode&);

	void bind_midi (MIDI::channel_t, MIDI::eventType, MIDI::byte);
	MIDI::channel_t get_control_channel () { return control_channel; }
	MIDI::eventType get_control_type () { return control_type; }
	MIDI::byte get_control_additional () { return control_additional; }
  private:
	PBD::Controllable& controllable;
	MIDI::Port&     _port;
	bool             setting;
	MIDI::byte       last_value;
	bool             bistate;
	int              midi_msg_id;      /* controller ID or note number */
	sigc::connection midi_sense_connection[2];
	sigc::connection midi_learn_connection;
	size_t           connections;
	MIDI::eventType  control_type;
	MIDI::byte       control_additional;
	MIDI::channel_t  control_channel;
	std::string     _control_description;
	bool             feedback;
	
	void midi_receiver (MIDI::Parser &p, MIDI::byte *, size_t);
	void midi_sense_note (MIDI::Parser &, MIDI::EventTwoBytes *, bool is_on);
	void midi_sense_note_on (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_note_off (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_controller (MIDI::Parser &, MIDI::EventTwoBytes *);
	void midi_sense_program_change (MIDI::Parser &, MIDI::byte);
	void midi_sense_pitchbend (MIDI::Parser &, MIDI::pitchbend_t);
};

#endif // __gm_midicontrollable_h__

