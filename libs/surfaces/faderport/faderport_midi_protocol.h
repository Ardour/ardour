/*
    Copyright (C) 2006 Paul Davis

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

#ifndef ardour_generic_midi_control_protocol_h
#define ardour_generic_midi_control_protocol_h

#include <list>
#include <glibmm/threads.h>

#include "ardour/types.h"

#include "control_protocol/control_protocol.h"

namespace PBD {
	class Controllable;
	class ControllableDescriptor;
}

#include <midi++/types.h>

//#include "pbd/signals.h"


//#include "midi_byte_array.h"
#include "types.h"

#include "glibmm/main.h"

namespace MIDI {
	class Parser;
	class Port;
}


namespace ARDOUR {
	class AsyncMIDIPort;
	class Port;
	class Session;
	class MidiPort;
}


class MIDIControllable;
class MIDIFunction;
class MIDIAction;

class FaderportMidiControlProtocol : public ARDOUR::ControlProtocol {
  public:
	FaderportMidiControlProtocol (ARDOUR::Session&);
	virtual ~FaderportMidiControlProtocol();

	int set_active (bool yn);
	static bool probe() { return true; }  //do SysEx device check here?

	void set_feedback_interval (ARDOUR::microseconds_t);

	int set_feedback (bool yn);
	bool get_feedback () const;

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	void set_current_bank (uint32_t);
	void next_bank ();
	void prev_bank ();

	void set_motorised (bool);

	bool motorised () const {
		return _motorised;
	}

	void set_threshold (int);

	int threshold () const {
		return _threshold;
	}

  private:
	MIDI::Port* _input_port;
	MIDI::Port* _output_port;
	boost::shared_ptr<ARDOUR::Port> _async_in;
	boost::shared_ptr<ARDOUR::Port> _async_out;

	ARDOUR::microseconds_t _feedback_interval;
	ARDOUR::microseconds_t last_feedback_time;
	int native_counter;

	bool  do_feedback;
	void  send_feedback ();

	PBD::ScopedConnection midi_recv_connection;
	void midi_receiver (MIDI::Parser &p, MIDI::byte *, size_t);

	bool midi_input_handler (Glib::IOCondition ioc, ARDOUR::AsyncMIDIPort* port);

	std::string _current_binding;
	uint32_t _bank_size;
	uint32_t _current_bank;
	/** true if this surface is motorised.  If it is, we assume
	    that the surface's controls are never out of sync with
	    Ardour's state, so we don't have to take steps to avoid
	    values jumping around when things are not in sync.
	*/
	bool _motorised;
	int _threshold;

	mutable void *gui;
	void build_gui ();
};

#endif /* ardour_generic_midi_control_protocol_h */
