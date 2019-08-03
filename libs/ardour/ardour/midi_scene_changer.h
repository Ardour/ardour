/*
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __libardour_midi_scene_changer_h__
#define __libardour_midi_scene_changer_h__

#include <glibmm/threads.h>

#include "ardour/scene_changer.h"

namespace ARDOUR
{

class MIDISceneChange;

class MIDISceneChanger : public SceneChanger
{
    public:
	MIDISceneChanger (Session&);
	~MIDISceneChanger ();

	void run (samplepos_t start, samplepos_t end);
	void set_input_port (boost::shared_ptr<MidiPort>);
	void set_output_port (boost::shared_ptr<MidiPort>);

	uint8_t bank_at (samplepos_t, uint8_t channel);
	uint8_t program_at (samplepos_t, uint8_t channel);

	void set_recording (bool);
	void locate (samplepos_t);

	/** Signal emitted whenever any relevant MIDI input is detected.
	 */
	PBD::Signal0<void> MIDIInputActivity;

	/** Signal emitted whenever any relevant MIDI output is sent.
	 */
	PBD::Signal0<void> MIDIOutputActivity;

    private:
	typedef std::multimap<samplepos_t,boost::shared_ptr<MIDISceneChange> > Scenes;

	boost::shared_ptr<MidiPort> input_port;
	boost::shared_ptr<MidiPort> output_port;
	Glib::Threads::RWLock scene_lock;
	Scenes scenes;
	bool _recording;
	bool have_seen_bank_changes;
	samplepos_t last_program_message_time;
	unsigned short current_bank;
	int last_delivered_program;
	int last_delivered_bank;

	void gather (const Locations::LocationList&);
	bool recording () const;
	void jump_to (int bank, int program);
	void rt_deliver (MidiBuffer&, samplepos_t, boost::shared_ptr<MIDISceneChange>);
	void non_rt_deliver (boost::shared_ptr<MIDISceneChange>);

	void bank_change_input (MIDI::Parser&, unsigned short, int channel);
	void program_change_input (MIDI::Parser&, MIDI::byte, int channel);
	void locations_changed ();

	PBD::ScopedConnectionList incoming_connections;
};

} // namespace

#endif /* __libardour_midi_scene_changer_h__ */
