/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gm_midiinvokable_h__
#define __gm_midiinvokable_h__

#include <string>

#include "midi++/types.h"

#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/types.h"

namespace MIDI {
	class Channel;
	class Parser;
}

class GenericMidiControlProtocol;

class MIDIInvokable : public PBD::Stateful
{
  public:
	MIDIInvokable (MIDI::Parser&);
	virtual ~MIDIInvokable ();

	virtual int init (GenericMidiControlProtocol&, const std::string&, MIDI::byte* data = 0, size_t dsize = 0);

	MIDI::Parser& get_parser() { return _parser; }

	void bind_midi (MIDI::channel_t, MIDI::eventType, MIDI::byte);
	MIDI::channel_t get_control_channel () { return control_channel; }
	MIDI::eventType get_control_type () { return control_type; }
	MIDI::byte get_control_additional () { return control_additional; }

  protected:
	GenericMidiControlProtocol* _ui;
	std::string     _invokable_name;
	MIDI::Parser&     _parser;
	PBD::ScopedConnection midi_sense_connection[2];
	MIDI::eventType  control_type;
	MIDI::byte       control_additional;
	MIDI::channel_t  control_channel;
	MIDI::byte*      data;
	size_t           data_size;
	bool            _parameterized;

	void midi_sense_note (MIDI::Parser &, MIDI::EventTwoBytes *, bool is_on);
	void midi_sense_note_on (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_note_off (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_controller (MIDI::Parser &, MIDI::EventTwoBytes *);
	void midi_sense_program_change (MIDI::Parser &, MIDI::byte);
	void midi_sense_sysex (MIDI::Parser &, MIDI::byte*, size_t);
	void midi_sense_any (MIDI::Parser &, MIDI::byte*, size_t);

	virtual void execute () = 0;
};

#endif // __gm_midicontrollable_h__

