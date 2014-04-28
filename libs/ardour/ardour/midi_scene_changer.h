/*
    Copyright (C) 2014 Paul Davis

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

#ifndef __libardour_midi_scene_changer_h__
#define __libardour_midi_scene_changer_h__

#include "ardour/scene_changer.h"

namespace ARDOUR
{

class MIDISceneChanger : public SceneChanger
{
    public:
	MIDISceneChanger (Session&);
	~MIDISceneChanger ();

	void run (framepos_t start, framepos_t end);
	void set_input_port (MIDI::Port*);
	void set_output_port (boost::shared_ptr<MidiPort>);

	uint8_t bank_at (framepos_t, uint8_t channel);
	uint8_t program_at (framepos_t, uint8_t channel);

	void set_recording (bool);
	void locate (framepos_t);

    private:
	typedef std::multimap<framepos_t,boost::shared_ptr<MIDISceneChange> > Scenes;

	MIDI::Port* input_port;
	boost::shared_ptr<MidiPort> output_port;
	Scenes scenes;
	bool _recording;
	framepos_t last_bank_message_time;
	framepos_t last_program_message_time;
	unsigned short current_bank;
	int last_delivered_program;
	int last_delivered_bank;

	void gather ();
	bool recording () const;
	void jump_to (int bank, int program);
	void deliver (MidiBuffer&, framepos_t, boost::shared_ptr<MIDISceneChange>);

	void bank_change_input (MIDI::Parser&, unsigned short);
	void program_change_input (MIDI::Parser&, MIDI::byte);
	void locations_changed (Locations::Change);

	PBD::ScopedConnection incoming_bank_change_connection;
	PBD::ScopedConnection incoming_program_change_connection;
};

} // namespace

#endif /* __libardour_midi_scene_changer_h__ */
