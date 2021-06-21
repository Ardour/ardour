/*
 * Copyright (C) 2006-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Len Ovens <len@ovenwerks.net>
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

#ifndef __gm_midicontrollable_h__
#define __gm_midicontrollable_h__

#include <string>

#include "midi++/types.h"

#include "pbd/controllable.h"
#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/types.h"

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
	MIDIControllable (GenericMidiControlProtocol*, MIDI::Parser&, boost::shared_ptr<PBD::Controllable>, bool momentary);
	MIDIControllable (GenericMidiControlProtocol*, MIDI::Parser&, bool momentary = false);
	virtual ~MIDIControllable ();

	int init (const std::string&);

	void rediscover_controllable ();
	bool bank_relative() const { return _bank_relative; }
	uint32_t rid() const { return _rid; }
	std::string what() const { return _what; }

	enum CtlType {
		Ctl_Momentary,
		Ctl_Toggle,
		Ctl_Dial,
	};

	enum Encoder {
		No_enc,
		Enc_R,
		Enc_L,
		Enc_2,
		Enc_B,
	};

	MIDI::byte* write_feedback (MIDI::byte* buf, int32_t& bufsize, bool force = false);

	void midi_rebind (MIDI::channel_t channel=-1);
	void midi_forget ();
	void learn_about_external_control ();
	void stop_learning ();
	void drop_external_control ();

	int control_to_midi(float val);
	float midi_to_control(int val);

	bool learned() const { return _learned; }

	CtlType get_ctltype() const { return _ctltype; }
	void set_ctltype (CtlType val) { _ctltype = val; }

	Encoder get_encoder() const { return _encoder; }
	void set_encoder (Encoder val) { _encoder = val; }

	MIDI::Parser& get_parser() { return _parser; }
	void set_controllable (boost::shared_ptr<PBD::Controllable>);
	boost::shared_ptr<PBD::Controllable> get_controllable () const;
	const std::string& current_uri() const { return _current_uri; }

	std::string control_description() const { return _control_description; }

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void bind_midi (MIDI::channel_t, MIDI::eventType, MIDI::byte);
	void bind_rpn_value (MIDI::channel_t, uint16_t rpn);
	void bind_nrpn_value (MIDI::channel_t, uint16_t rpn);
	void bind_rpn_change (MIDI::channel_t, uint16_t rpn);
	void bind_nrpn_change (MIDI::channel_t, uint16_t rpn);

	MIDI::channel_t get_control_channel () { return control_channel; }
	MIDI::eventType get_control_type () { return control_type; }
	MIDI::byte get_control_additional () { return control_additional; }

	int lookup_controllable();

private:

	int max_value_for_type () const;

	GenericMidiControlProtocol* _surface;
	boost::shared_ptr<PBD::Controllable> _controllable;
	std::string     _current_uri;
	MIDI::Parser&   _parser;
	bool             setting;
	int              last_value;
	int              last_incoming;
	float            last_controllable_value;
	bool            _momentary;
	bool            _is_gain_controller;
	bool            _learned;
	CtlType         _ctltype;
	Encoder			_encoder;
	int              midi_msg_id;      /* controller ID or note number */
	PBD::ScopedConnection midi_sense_connection[2];
	PBD::ScopedConnection midi_learn_connection;
	PBD::ScopedConnection controllable_death_connection;
	/** the type of MIDI message that is used for this control */
	MIDI::eventType  control_type;
	MIDI::byte       control_additional;
	MIDI::channel_t  control_channel;
	std::string     _control_description;
	int16_t          control_rpn;
	int16_t          control_nrpn;
	uint32_t        _rid;
	std::string     _what;
	bool            _bank_relative;

	void drop_controllable ();
	Glib::Threads::Mutex controllable_lock;

	void midi_receiver (MIDI::Parser &p, MIDI::byte *, size_t);
	void midi_sense_note (MIDI::Parser &, MIDI::EventTwoBytes *, bool is_on);
	void midi_sense_note_on (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_note_off (MIDI::Parser &p, MIDI::EventTwoBytes *tb);
	void midi_sense_controller (MIDI::Parser &, MIDI::EventTwoBytes *);
	void midi_sense_program_change (MIDI::Parser &, MIDI::byte);
	void midi_sense_pitchbend (MIDI::Parser &, MIDI::pitchbend_t);

	void nrpn_value_change (MIDI::Parser&, uint16_t nrpn, float val);
	void rpn_value_change (MIDI::Parser&, uint16_t nrpn, float val);
	void rpn_change (MIDI::Parser&, uint16_t nrpn, int direction);
	void nrpn_change (MIDI::Parser&, uint16_t nrpn, int direction);
};

#endif // __gm_midicontrollable_h__
