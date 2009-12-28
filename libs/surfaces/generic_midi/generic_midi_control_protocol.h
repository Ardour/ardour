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

#include <set>
#include <list>
#include <glibmm/thread.h>
#include "ardour/types.h"

#include "control_protocol/control_protocol.h"

namespace MIDI {
	class Port;
}

namespace PBD {
	class Controllable;
}	

namespace ARDOUR {
	class Session;
}

class MIDIControllable;
class MIDIFunction;

class GenericMidiControlProtocol : public ARDOUR::ControlProtocol {
  public:
	GenericMidiControlProtocol (ARDOUR::Session&);
	virtual ~GenericMidiControlProtocol();

	int set_active (bool yn);
	static bool probe() { return true; }

	MIDI::Port* port () const { return _port; }
	void set_feedback_interval (ARDOUR::microseconds_t);

	int set_feedback (bool yn);
	bool get_feedback () const;

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

  private:
	MIDI::Port* _port;
	ARDOUR::microseconds_t _feedback_interval;
	ARDOUR::microseconds_t last_feedback_time;

	bool  do_feedback;
	void _send_feedback ();
	void  send_feedback ();

	typedef std::set<MIDIControllable*> MIDIControllables;
	MIDIControllables controllables;

	typedef std::list<MIDIFunction*> MIDIFunctions;
	MIDIFunctions functions;

	typedef std::pair<MIDIControllable*,PBD::Connection> MIDIPendingControllable;
	typedef std::list<MIDIPendingControllable* > MIDIPendingControllables;
	MIDIPendingControllables pending_controllables;
	Glib::Mutex controllables_lock;
	Glib::Mutex pending_lock;

	bool start_learning (PBD::Controllable*);
	void stop_learning (PBD::Controllable*);

	void learning_stopped (MIDIControllable*);

	void create_binding (PBD::Controllable*, int, int);
	void delete_binding (PBD::Controllable*);

	int load_bindings (const std::string&);
	MIDIControllable* create_binding (const XMLNode&);
	MIDIFunction* create_function (const XMLNode&);

	void reset_controllables ();
};

#endif /* ardour_generic_midi_control_protocol_h */
