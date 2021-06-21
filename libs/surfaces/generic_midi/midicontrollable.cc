/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <stdint.h>
#include <cmath>
#include <climits>
#include <iostream>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/types_convert.h"
#include "pbd/xml++.h"

#include "midi++/types.h" // Added by JE - 06-01-2009. All instances of 'byte' changed to 'MIDI::byte' (for clarification)
#include "midi++/port.h"
#include "midi++/channel.h"

#include "ardour/async_midi_port.h"
#include "ardour/automation_control.h"
#include "ardour/midi_ui.h"
#include "ardour/debug.h"

#include "midicontrollable.h"
#include "generic_midi_control_protocol.h"

using namespace std;
using namespace MIDI;
using namespace PBD;
using namespace ARDOUR;

MIDIControllable::MIDIControllable (GenericMidiControlProtocol* s, MIDI::Parser& p, bool m)
	: _surface (s)
	, _parser (p)
	, _momentary (m)
{
	_learned = false; /* from URI */
	_ctltype = Ctl_Momentary;
	_encoder = No_enc;
	setting = false;
	last_value = 0; // got a better idea ?
	last_incoming = 256; // any out of band value
	last_controllable_value = 0.0f;
	control_type = none;
	control_rpn = -1;
	control_nrpn = -1;
	_control_description = "MIDI Control: none";
	control_additional = (MIDI::byte) -1;
}

MIDIControllable::MIDIControllable (GenericMidiControlProtocol* s, MIDI::Parser& p, boost::shared_ptr<PBD::Controllable> c, bool m)
	: _surface (s)
	, _parser (p)
	, _momentary (m)
{
	set_controllable (c);

	_learned = true; /* from controllable */
	_ctltype = Ctl_Momentary;
	_encoder = No_enc;
	setting = false;
	last_value = 0; // got a better idea ?
	last_controllable_value = 0.0f;
	control_type = none;
	control_rpn = -1;
	control_nrpn = -1;
	_control_description = "MIDI Control: none";
	control_additional = (MIDI::byte) -1;
}

MIDIControllable::~MIDIControllable ()
{
	drop_external_control ();
}

int
MIDIControllable::init (const std::string& s)
{
	_current_uri = s;
	return 0;
}

void
MIDIControllable::midi_forget ()
{
	/* stop listening for incoming messages, but retain
	   our existing event + type information.
	*/

	midi_sense_connection[0].disconnect ();
	midi_sense_connection[1].disconnect ();
	midi_learn_connection.disconnect ();
}

void
MIDIControllable::drop_external_control ()
{
	midi_forget ();
	control_rpn = -1;
	control_nrpn = -1;
	control_type = none;
	control_additional = (MIDI::byte) -1;
}

boost::shared_ptr<PBD::Controllable>
MIDIControllable::get_controllable () const
{
	return _controllable;
}

void
MIDIControllable::set_controllable (boost::shared_ptr<PBD::Controllable> c)
{
	Glib::Threads::Mutex::Lock lm (controllable_lock);
	if (c && c == _controllable) {
		return;
	}

	controllable_death_connection.disconnect ();

	if (c) {
		_controllable = c;
		last_controllable_value = control_to_midi (c->get_value());
	} else {
		_controllable.reset();
		last_controllable_value = 0.0f; // is there a better value?
	}

	last_incoming = 256;

	if (c) {
		c->DropReferences.connect_same_thread (controllable_death_connection, boost::bind (&MIDIControllable::drop_controllable, this));
	}
}

void
MIDIControllable::midi_rebind (channel_t c)
{
	if (c >= 0) {
		bind_midi (c, control_type, control_additional);
	} else {
		midi_forget ();
	}
}

void
MIDIControllable::learn_about_external_control ()
{
	drop_external_control ();
	_parser.any.connect_same_thread (midi_learn_connection, boost::bind (&MIDIControllable::midi_receiver, this, _1, _2, _3));
}

void
MIDIControllable::stop_learning ()
{
	midi_learn_connection.disconnect ();
}

int
MIDIControllable::control_to_midi (float val)
{
	if (!_controllable) {
		return 0;
	}

	if (_controllable->is_gain_like()) {
		return _controllable->internal_to_interface (val) * max_value_for_type ();
	}

	float control_min = _controllable->lower ();
	float control_max = _controllable->upper ();
	float control_range = control_max - control_min;

	if (_controllable->is_toggle()) {
		if (val >= (control_min + (control_range/2.0f))) {
			return max_value_for_type();
		} else {
			return 0;
		}
	} else {
		boost::shared_ptr<AutomationControl> actl = boost::dynamic_pointer_cast<AutomationControl> (_controllable);
		if (actl) {
			control_min = actl->internal_to_interface(control_min);
			control_max = actl->internal_to_interface(control_max);
			control_range = control_max - control_min;
			val = actl->internal_to_interface(val);
		}
	}
	// fiddle value of max so value doesn't jump from 125 to 127 for 1.0
	// otherwise decrement won't work.
	return (val - control_min) / control_range * (max_value_for_type () - 1);
}

float
MIDIControllable::midi_to_control (int val)
{
	if (!_controllable) {
		return 0;
	}
	/* fiddle with MIDI value so that we get an odd number of integer steps
		and can thus represent "middle" precisely as 0.5. this maps to
		the range 0..+1.0 (0 to 126)
	*/

	float fv = (val == 0 ? 0 : float (val - 1) / (max_value_for_type() - 1));

	if (_controllable->is_gain_like()) {
		return _controllable->interface_to_internal (fv);
	}
	DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Raw value %1 float %2\n", val, fv));

	float control_min = _controllable->lower ();
	float control_max = _controllable->upper ();
	float control_range = control_max - control_min;
	DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Min %1 Max %2 Range %3\n", control_min, control_max, control_range));

	boost::shared_ptr<AutomationControl> actl = boost::dynamic_pointer_cast<AutomationControl> (_controllable);
	if (actl) {
		if (fv == 0.f) return control_min;
		if (fv == 1.f) return control_max;
		control_min = actl->internal_to_interface(control_min);
		control_max = actl->internal_to_interface(control_max);
		control_range = control_max - control_min;
		return actl->interface_to_internal((fv * control_range) + control_min);
	}
	return (fv * control_range) + control_min;
}

void
MIDIControllable::midi_sense_note_on (Parser &p, EventTwoBytes *tb)
{
	midi_sense_note (p, tb, true);
}

void
MIDIControllable::midi_sense_note_off (Parser &p, EventTwoBytes *tb)
{
	midi_sense_note (p, tb, false);
}

int
MIDIControllable::lookup_controllable()
{
	if (_current_uri.empty()) {
		return -1;
	}

	boost::shared_ptr<Controllable> c = _surface->lookup_controllable (_current_uri);

	if (!c) {
		set_controllable (boost::shared_ptr<PBD::Controllable>());
		return -1;
	}

	set_controllable (c);

	return 0;
}

void
MIDIControllable::drop_controllable ()
{
	set_controllable (boost::shared_ptr<PBD::Controllable>());
}

void
MIDIControllable::midi_sense_note (Parser &, EventTwoBytes *msg, bool /*is_on*/)
{
	if (!_controllable) {
		if (lookup_controllable()) {
			return;
		}
	}

	assert (_controllable);

	_surface->maybe_start_touch (_controllable);

	if (!_controllable->is_toggle()) {
		if (control_additional == msg->note_number) {
			_controllable->set_value (midi_to_control (msg->velocity), Controllable::UseGroup);
			DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Note %1 value %2  %3\n", (int) msg->note_number, (float) midi_to_control (msg->velocity), current_uri() ));
		}
	} else {
		if (control_additional == msg->note_number) {
			float new_value = _controllable->get_value() > 0.5f ? 0.0f : 1.0f;
			_controllable->set_value (new_value, Controllable::UseGroup);
			DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Note %1 Value %2  %3\n", (int) msg->note_number, (float) new_value, current_uri()));
		}
	}

	last_value = (MIDI::byte) (_controllable->get_value() * 127.0); // to prevent feedback fights
}

void
MIDIControllable::midi_sense_controller (Parser &, EventTwoBytes *msg)
{
	if (!_controllable) {
		if (lookup_controllable ()) {
			return;
		}
	}

	assert (_controllable);

	_surface->maybe_start_touch (_controllable);

	if (control_additional == msg->controller_number) {

		if (!_controllable->is_toggle()) {
			if (get_encoder() == No_enc) {
				float new_value = msg->value;
				float max_value = max(last_controllable_value, new_value);
				float min_value = min(last_controllable_value, new_value);
				float range = max_value - min_value;
				float threshold = (float) _surface->threshold ();

				bool const in_sync = (
					range < threshold &&
					_controllable->get_value() <= midi_to_control(max_value) &&
					_controllable->get_value() >= midi_to_control(min_value)
					);

				/* If the surface is not motorised, we try to prevent jumps when
				   the MIDI controller and controllable are out of sync.
				   There might be a better way of doing this.
				*/

				if (in_sync || _surface->motorised ()) {
					_controllable->set_value (midi_to_control (new_value), Controllable::UseGroup);
				}
				DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("MIDI CC %1 value %2  %3\n", (int) msg->controller_number, (float) midi_to_control(new_value), current_uri() ));

				last_controllable_value = new_value;
			} else {
				uint32_t cur_val = control_to_midi(_controllable->get_value ());
				int offset = (msg->value & 0x3f);
				switch (get_encoder()) {
					case Enc_L:
						if (msg->value & 0x40) {
							_controllable->set_value (midi_to_control (cur_val - offset), Controllable::UseGroup);
						} else {
							_controllable->set_value (midi_to_control (cur_val + offset + 1), Controllable::UseGroup);
						}
						break;
					case Enc_R:
						if (msg->value & 0x40) {
							_controllable->set_value (midi_to_control (cur_val + offset + 1), Controllable::UseGroup);
						} else {
							_controllable->set_value (midi_to_control (cur_val - offset), Controllable::UseGroup);
						}
						break;
					case Enc_2:
						// 0x40 is max pos offset
						if (msg->value > 0x40) {
							_controllable->set_value (midi_to_control (cur_val - (0x7f - msg->value)), Controllable::UseGroup);
						} else {
							_controllable->set_value (midi_to_control (cur_val + msg->value + 1), Controllable::UseGroup);
						}
						break;
					case Enc_B:
						if (msg->value > 0x40) {
							_controllable->set_value (midi_to_control (cur_val + offset + 1), Controllable::UseGroup);
						} else if (msg->value < 0x40) {
							_controllable->set_value (midi_to_control (cur_val - (0x40 - msg->value)), Controllable::UseGroup);
						} // 0x40 = 0 do nothing
						break;
					default:
						break;
				}
				DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("MIDI CC %1 value %2  %3\n", (int) msg->controller_number, (int) cur_val, current_uri() ));

			}
		} else {

			switch (get_ctltype()) {
			case Ctl_Dial:
				/* toggle value whenever direction of knob motion changes */
				if (last_incoming > 127) {
					/* relax ... first incoming message */
				} else {
					if (msg->value > last_incoming) {
						_controllable->set_value (1.0, Controllable::UseGroup);
					} else {
						_controllable->set_value (0.0, Controllable::UseGroup);
					}
					DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("dial Midi CC %1 value 1  %2\n", (int) msg->controller_number, current_uri()));
				}
				last_incoming = msg->value;
				break;
			case Ctl_Momentary:
				/* toggle it if over 64, otherwise leave it alone. This behaviour that works with buttons which send a value > 64 each
				 * time they are pressed.
				 */
				if (msg->value >= 0x40) {
					_controllable->set_value (_controllable->get_value() >= 0.5 ? 0.0 : 1.0, Controllable::UseGroup);
					DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("toggle Midi CC %1 value 1  %2\n", (int) msg->controller_number, current_uri()));
				}
				break;
			case Ctl_Toggle:
				/* toggle if value is over 64, otherwise turn it off. This is behaviour designed for buttons which send a value > 64 when pressed,
				   maintain state (i.e. they know they were pressed) and then send zero the next time.
				*/
				if (msg->value >= 0x40) {
					_controllable->set_value (_controllable->get_value() >= 0.5 ? 0.0 : 1.0, Controllable::UseGroup);
				} else {
					_controllable->set_value (0.0, Controllable::NoGroup);
					DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Midi CC %1 value 0  %2\n", (int) msg->controller_number, current_uri()));
					break;
				}
			}
		}
	}
}

void
MIDIControllable::midi_sense_program_change (Parser &, MIDI::byte msg)
{
	if (!_controllable) {
		if (lookup_controllable ()) {
			return;
		}
	}

	assert (_controllable);

	_surface->maybe_start_touch (_controllable);

	if (msg == control_additional) {

		if (!_controllable->is_toggle()) {
			_controllable->set_value (1.0, Controllable::UseGroup);
			DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("MIDI program %1 value 1.0  %3\n", (int) msg, current_uri() ));
		} else  {
			float new_value = _controllable->get_value() > 0.5f ? 0.0f : 1.0f;
			_controllable->set_value (new_value, Controllable::UseGroup);
			DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("MIDI program %1 value %2  %3\n", (int) msg, (float) new_value, current_uri()));
		}
	}

	last_value = (MIDI::byte) (_controllable->get_value() * 127.0); // to prevent feedback fights
}

void
MIDIControllable::midi_sense_pitchbend (Parser &, pitchbend_t pb)
{
	if (!_controllable) {
		if (lookup_controllable ()) {
			return;
		}
	}

	assert (_controllable);

	_surface->maybe_start_touch (_controllable);

	if (!_controllable->is_toggle()) {

		float new_value = pb;
		float max_value = max (last_controllable_value, new_value);
		float min_value = min (last_controllable_value, new_value);
		float range = max_value - min_value;
		float threshold = 128.f * _surface->threshold ();

		bool const in_sync = (
				range < threshold &&
				_controllable->get_value() <= midi_to_control (max_value) &&
				_controllable->get_value() >= midi_to_control (min_value)
				);

		if (in_sync || _surface->motorised ()) {
			_controllable->set_value (midi_to_control (pb), Controllable::UseGroup);
		}

		DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("MIDI pitchbend %1 value %2  %3\n", (int) control_channel, (float) midi_to_control (pb), current_uri() ));
		last_controllable_value = new_value;
	} else {
		if (pb > 8065.0f) {
			_controllable->set_value (1, Controllable::UseGroup);
			DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Midi pitchbend %1 value 1  %2\n", (int) control_channel, current_uri()));
		} else {
			_controllable->set_value (0, Controllable::UseGroup);
			DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Midi pitchbend %1 value 0  %2\n", (int) control_channel, current_uri()));
		}
	}

	last_value = control_to_midi (_controllable->get_value ());
}

void
MIDIControllable::midi_receiver (Parser &, MIDI::byte *msg, size_t /*len*/)
{
	/* we only respond to channel messages */

	if ((msg[0] & 0xF0) < 0x80 || (msg[0] & 0xF0) > 0xE0) {
		return;
	}

	_surface->check_used_event(msg[0], msg[1]);
	bind_midi ((channel_t) (msg[0] & 0xf), eventType (msg[0] & 0xF0), msg[1]);

	if (_controllable) {
		_controllable->LearningFinished ();
	}
}

void
MIDIControllable::rpn_value_change (Parser&, uint16_t rpn, float val)
{
	if (control_rpn == rpn) {
		if (_controllable) {
			_controllable->set_value (val, Controllable::UseGroup);
		}
	}
}

void
MIDIControllable::nrpn_value_change (Parser&, uint16_t nrpn, float val)
{
	if (control_nrpn == nrpn) {
		if (_controllable) {
			_controllable->set_value (val, Controllable::UseGroup);
		}
	}
}

void
MIDIControllable::rpn_change (Parser&, uint16_t rpn, int dir)
{
	if (control_rpn == rpn) {
		if (_controllable) {
			/* XXX how to increment/decrement ? */
			// _controllable->set_value (val);
		}
	}
}

void
MIDIControllable::nrpn_change (Parser&, uint16_t nrpn, int dir)
{
	if (control_nrpn == nrpn) {
		if (_controllable) {
			/* XXX how to increment/decrement ? */
			// _controllable->set_value (val);
		}
	}
}

void
MIDIControllable::bind_rpn_value (channel_t chn, uint16_t rpn)
{
	int chn_i = chn;
	drop_external_control ();
	control_rpn = rpn;
	control_channel = chn;
	_parser.channel_rpn[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIControllable::rpn_value_change, this, _1, _2, _3));
}

void
MIDIControllable::bind_nrpn_value (channel_t chn, uint16_t nrpn)
{
	int chn_i = chn;
	drop_external_control ();
	control_nrpn = nrpn;
	control_channel = chn;
	_parser.channel_nrpn[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIControllable::rpn_value_change, this, _1, _2, _3));
}

void
MIDIControllable::bind_nrpn_change (channel_t chn, uint16_t nrpn)
{
	int chn_i = chn;
	drop_external_control ();
	control_nrpn = nrpn;
	control_channel = chn;
	_parser.channel_nrpn_change[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIControllable::rpn_change, this, _1, _2, _3));
}

void
MIDIControllable::bind_rpn_change (channel_t chn, uint16_t rpn)
{
	int chn_i = chn;
	drop_external_control ();
	control_rpn = rpn;
	control_channel = chn;
	_parser.channel_rpn_change[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIControllable::nrpn_change, this, _1, _2, _3));
}

void
MIDIControllable::bind_midi (channel_t chn, eventType ev, MIDI::byte additional)
{
	char buf[64];

	drop_external_control ();

	control_type = ev;
	control_channel = chn;
	control_additional = additional;

	int chn_i = chn;
	switch (ev) {
	case MIDI::off:
		_parser.channel_note_off[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIControllable::midi_sense_note_off, this, _1, _2));

		/* if this is a togglee, connect to noteOn as well,
		   and we'll toggle back and forth between the two.
		*/

		if (_momentary) {
			_parser.channel_note_on[chn_i].connect_same_thread (midi_sense_connection[1], boost::bind (&MIDIControllable::midi_sense_note_on, this, _1, _2));
		}

		_control_description = "MIDI control: NoteOff";
		break;

	case MIDI::on:
		_parser.channel_note_on[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIControllable::midi_sense_note_on, this, _1, _2));
		if (_momentary) {
			_parser.channel_note_off[chn_i].connect_same_thread (midi_sense_connection[1], boost::bind (&MIDIControllable::midi_sense_note_off, this, _1, _2));
		}
		_control_description = "MIDI control: NoteOn";
		break;

	case MIDI::controller:
		_parser.channel_controller[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIControllable::midi_sense_controller, this, _1, _2));
		snprintf (buf, sizeof (buf), "MIDI control: Controller %d", control_additional);
		_control_description = buf;
		break;

	case MIDI::program:
		_parser.channel_program_change[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIControllable::midi_sense_program_change, this, _1, _2));
		_control_description = "MIDI control: ProgramChange";
		break;

	case MIDI::pitchbend:
		_parser.channel_pitchbend[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIControllable::midi_sense_pitchbend, this, _1, _2));
		_control_description = "MIDI control: Pitchbend";
		break;

	default:
		break;
	}
	DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Controlable: bind_midi: %1 on Channel %2 value %3 \n", _control_description, chn_i + 1, (int) additional));
}

MIDI::byte*
MIDIControllable::write_feedback (MIDI::byte* buf, int32_t& bufsize, bool /*force*/)
{
	Glib::Threads::Mutex::Lock lm (controllable_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked ()) {
		return buf;
	}
	if (!_controllable || !_surface->get_feedback ()) {
		return buf;
	}

	float val = _controllable->get_value ();

	/* Note that when sending RPN/NPRN we do two things:
	 *
	 * always send MSB first, then LSB
	 * null/reset the parameter ID after sending.
	 *
	 * this follows recommendations found online, eg. http://www.philrees.co.uk/nrpnq.htm
	 */

	if (control_rpn >= 0) {
		if (bufsize < 13) {
			return buf;
		}
		int rpn_val = (int) lrintf (val * 16383.0);
		if (last_value == rpn_val) {
			return buf;
		}
		*buf++ = (0xb0) | control_channel;
		*buf++ = 0x62;
		*buf++ = (int) ((control_rpn) >> 7);
		*buf++ = 0x63;
		*buf++ = (int) (control_rpn & 0x7f);
		*buf++ = 0x06;
		*buf++ = (int) (rpn_val >> 7);
		*buf++ = 0x26;
		*buf++ = (int) (rpn_val & 0x7f);
		*buf++ = 0x62;
		*buf++ = 0x7f;
		*buf++ = 0x63;
		*buf++ = 0x7f;
		bufsize -= 13;
		last_value = rpn_val;
		DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("MIDI out: RPN %1 Channel %2 Value %3\n", control_rpn, (int) control_channel, val));
		return buf;
	}

	if (control_nrpn >= 0) {
		int rpn_val = (int) lrintf (val * 16383.0);
		if (last_value == rpn_val) {
			return buf;
		}
		*buf++ = (0xb0) | control_channel;
		*buf++ = 0x64;
		*buf++ = (int) ((control_rpn) >> 7);
		*buf++ = 0x65;
		*buf++ = (int) (control_rpn & 0x7f);
		*buf++ = 0x06;
		*buf++ = (int) (rpn_val >> 7);
		*buf++ = 0x26;
		*buf++ = (int) (rpn_val & 0x7f);
		*buf++ = 0x64;
		*buf++ = 0x7f;
		*buf++ = 0x65;
		*buf++ = 0x7f;
		last_value = rpn_val;
		bufsize -= 13;
		DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("MIDI out: NRPN %1 Channel %2 Value %3\n", control_nrpn, (int) control_channel, val));
		return buf;
	}

	if (control_type == none || bufsize <= 2) {
		return buf;
	}

	int const gm = control_to_midi (val);

	if (gm == last_value) {
		return buf;
	}

	DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Feedback: %1 %2\n", control_description(), current_uri()));

	*buf++ = (0xF0 & control_type) | (0xF & control_channel);
	int ev_size = 3;
	switch (control_type) {
	case MIDI::pitchbend:
		*buf++ = int (gm) & 127;
		*buf++ = (int (gm) >> 7) & 127;
		break;
	case MIDI::program:
		*buf++ = control_additional; /* program number */
		ev_size = 2;
		break;
	default:
		*buf++ = control_additional; /* controller number */
		*buf++ = gm;
		break;
	}
	DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("MIDI out: Type %1 Channel %2 Bytes %3 %4\n", (int) control_type, (int) control_channel , (int) *(buf - 2), (int) *(buf - 1)));

	last_value = gm;
	bufsize -= ev_size;

	return buf;
}

int
MIDIControllable::set_state (const XMLNode& node, int /*version*/)
{
	int xx;

	std::string str;
	if (node.get_property ("event", str)) {
		sscanf (str.c_str(), "0x%x", &xx);
		control_type = (MIDI::eventType) xx;
	} else {
		return -1;
	}

	if (node.get_property ("channel", xx)) {
		control_channel = xx;
	} else {
		return -1;
	}

	if (node.get_property ("additional", str)) {
		sscanf (str.c_str(), "0x%x", &xx);
		control_additional = (MIDI::byte) xx;
	} else {
		return -1;
	}

	bind_midi (control_channel, control_type, control_additional);

	return 0;
}

XMLNode&
MIDIControllable::get_state ()
{
	char buf[32];

	XMLNode* node = new XMLNode ("MIDIControllable");

	if (_current_uri.empty() && _controllable) {
		node->set_property ("id", _controllable->id ());
	} else {
		node->set_property ("uri", _current_uri);
	}

	if (_controllable) {
		snprintf (buf, sizeof(buf), "0x%x", (int) control_type);
		node->set_property ("event", (const char *)buf);
		node->set_property ("channel", (int16_t)control_channel);
		snprintf (buf, sizeof(buf), "0x%x", (int) control_additional);
		node->set_property ("additional", (const char *)buf);
	}

	return *node;
}

/** @return the maximum value for a control value transmitted
 *  using a given MIDI::eventType.
 */
int
MIDIControllable::max_value_for_type () const
{
	/* XXX: this is not complete */

	if (control_type == MIDI::pitchbend) {
		return 16383;
	}

	return 127;
}
