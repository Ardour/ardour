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
#include <glibmm/thread.h>
#include "ardour/types.h"

#include "control_protocol/control_protocol.h"

namespace MIDI {
	class Port;
}

namespace PBD {
	class Controllable;
	class ControllableDescriptor;
}	

namespace ARDOUR {
	class Session;
}

class MIDIControllable;
class MIDIFunction;
class MIDIAction;

class GenericMidiControlProtocol : public ARDOUR::ControlProtocol {
  public:
	GenericMidiControlProtocol (ARDOUR::Session&);
	virtual ~GenericMidiControlProtocol();

	int set_active (bool yn);
	static bool probe() { return true; }

	MIDI::Port* input_port () const { return _input_port; }
	MIDI::Port* output_port () const { return _output_port; }
	void set_feedback_interval (ARDOUR::microseconds_t);

	int set_feedback (bool yn);
	bool get_feedback () const;

        boost::shared_ptr<PBD::Controllable> lookup_controllable (const PBD::ControllableDescriptor&) const;

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	int load_bindings (const std::string&);
	void drop_bindings ();
	
	std::string current_binding() const { return _current_binding; }

	struct MapInfo {
	    std::string name;
	    std::string path;
	};

	std::list<MapInfo> map_info;
	void reload_maps ();

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
	ARDOUR::microseconds_t _feedback_interval;
	ARDOUR::microseconds_t last_feedback_time;

	bool  do_feedback;
	void _send_feedback ();
	void  send_feedback ();

	typedef std::list<MIDIControllable*> MIDIControllables;
	MIDIControllables controllables;

	typedef std::list<MIDIFunction*> MIDIFunctions;
	MIDIFunctions functions;

	typedef std::list<MIDIAction*> MIDIActions;
	MIDIActions actions;

	typedef std::pair<MIDIControllable*,PBD::ScopedConnection> MIDIPendingControllable;
	typedef std::list<MIDIPendingControllable* > MIDIPendingControllables;
	MIDIPendingControllables pending_controllables;
	Glib::Mutex controllables_lock;
	Glib::Mutex pending_lock;

	bool start_learning (PBD::Controllable*);
	void stop_learning (PBD::Controllable*);

	void learning_stopped (MIDIControllable*);

	void create_binding (PBD::Controllable*, int, int);
	void delete_binding (PBD::Controllable*);

	MIDIControllable* create_binding (const XMLNode&);
	MIDIFunction* create_function (const XMLNode&);
	MIDIAction* create_action (const XMLNode&);

	void reset_controllables ();
	void drop_all ();

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
