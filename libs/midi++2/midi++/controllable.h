/*
    Copyright (C) 1998-99 Paul Barton-Davis
 
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

    $Id$
*/

#ifndef __qm_midicontrollable_h__
#define __qm_midicontrollable_h__

#include <string>

#include <sigc++/sigc++.h>

#include <midi++/types.h>

namespace MIDI {

class Channel;
class Port;
class Parser;

class Controllable : public sigc::trackable
{
  public:
	Controllable (Port *, bool bistate = false);
	virtual ~Controllable ();

	void midi_rebind (Port *, channel_t channel=-1);
	void midi_forget ();
	void learn_about_external_control ();
	void stop_learning ();
	void drop_external_control ();

	virtual void set_value (float) = 0;

	sigc::signal<void> learning_started;
	sigc::signal<void> learning_stopped;

	bool get_control_info (channel_t&, eventType&, byte&);
	void set_control_type (channel_t, eventType, byte);

	bool get_midi_feedback () { return feedback; }
	void set_midi_feedback (bool val) { feedback = val; }

	Port * get_port() { return port; }
	
	std::string control_description() const { return _control_description; }

	void send_midi_feedback (float val, timestamp_t timestamp);
	
  private:
	bool             bistate;
	int              midi_msg_id;      /* controller ID or note number */
	sigc::connection midi_sense_connection[2];
	sigc::connection midi_learn_connection;
	size_t           connections;
	Port*            port;
	eventType        control_type;
	byte             control_additional;
	channel_t        control_channel;
	std::string     _control_description;
	bool             feedback;
	
	void midi_receiver (Parser &p, byte *, size_t);
	void midi_sense_note (Parser &, EventTwoBytes *, bool is_on);
	void midi_sense_note_on (Parser &p, EventTwoBytes *tb);
	void midi_sense_note_off (Parser &p, EventTwoBytes *tb);
	void midi_sense_controller (Parser &, EventTwoBytes *);
	void midi_sense_program_change (Parser &, byte);
	void midi_sense_pitchbend (Parser &, pitchbend_t);

	void bind_midi (channel_t, eventType, byte);
};

}; /* namespace MIDI */

#endif // __qm_midicontrollable_h__

