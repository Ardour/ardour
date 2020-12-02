/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#ifndef ardour_generic_midi_control_protocol_h
#define ardour_generic_midi_control_protocol_h

#include <list>
#include <glibmm/threads.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "ardour/types.h"
#include "ardour/port.h"

#include "control_protocol/control_protocol.h"

namespace PBD {
	class Controllable;
}

namespace ARDOUR {
	class AsyncMIDIPort;
	class MidiPort;
	class Session;
}

namespace MIDI {
	class Port;
}

class MIDIControllable;
class MIDIFunction;
class MIDIAction;

struct GenericMIDIRequest : public BaseUI::BaseRequestObject {
public:
	GenericMIDIRequest () {}
	~GenericMIDIRequest () {}
};


class GenericMidiControlProtocol : public ARDOUR::ControlProtocol, public AbstractUI<GenericMIDIRequest> {
public:
	GenericMidiControlProtocol (ARDOUR::Session&);
	virtual ~GenericMidiControlProtocol();

	void do_request (GenericMIDIRequest*);
	int stop ();

	void thread_init ();

	int set_active (bool yn);
	static bool probe() { return true; }

	void stripable_selection_changed () {}

	std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles ();

	boost::shared_ptr<ARDOUR::Port> input_port () const;
	boost::shared_ptr<ARDOUR::Port> output_port () const;

	void set_feedback_interval (PBD::microseconds_t);

	int set_feedback (bool yn);
	bool get_feedback () const;

	boost::shared_ptr<PBD::Controllable> lookup_controllable (std::string const &) const;

	void maybe_start_touch (boost::shared_ptr<PBD::Controllable>);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	int load_bindings (const std::string&);
	void drop_bindings ();

	void check_used_event (int, int);

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

	PBD::Signal0<void> ConnectionChange;

private:
	boost::shared_ptr<ARDOUR::Bundle> _input_bundle;
	boost::shared_ptr<ARDOUR::Bundle> _output_bundle;
	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _input_port;
	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _output_port;

	PBD::microseconds_t _feedback_interval;
	PBD::microseconds_t last_feedback_time;

	bool  do_feedback;
	void _send_feedback ();
	void  send_feedback ();

	typedef std::list<MIDIControllable*> MIDIControllables;
	MIDIControllables controllables;

	typedef std::list<MIDIFunction*> MIDIFunctions;
	MIDIFunctions functions;

	typedef std::list<MIDIAction*> MIDIActions;
	MIDIActions actions;

	struct MIDIPendingControllable {
		MIDIControllable* mc;
		bool own_mc;
		PBD::ScopedConnection connection;

		MIDIPendingControllable (MIDIControllable* c, bool omc)
			: mc (c)
			, own_mc (omc)
		{}
	};
	typedef std::list<MIDIPendingControllable* > MIDIPendingControllables;
	MIDIPendingControllables pending_controllables;
	Glib::Threads::Mutex controllables_lock;
	Glib::Threads::Mutex pending_lock;

	bool start_learning (boost::weak_ptr<PBD::Controllable>);
	void stop_learning (boost::weak_ptr<PBD::Controllable>);

	void learning_stopped (MIDIControllable*);

	MIDIControllable* create_binding (const XMLNode&);
	MIDIFunction* create_function (const XMLNode&);
	MIDIAction* create_action (const XMLNode&);

	void reset_controllables ();
	void drop_all ();

	enum ConnectionState {
		InputConnected = 0x1,
		OutputConnected = 0x2
	};

	int connection_state;
	bool connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn);
	PBD::ScopedConnection _port_connection;

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

	PBD::ScopedConnectionList midi_connections;

	bool midi_input_handler (Glib::IOCondition ioc, boost::weak_ptr<ARDOUR::AsyncMIDIPort> port);
	void start_midi_handling ();
	void stop_midi_handling ();

	void event_loop_precall () { ControlProtocol::event_loop_precall(); }
};

#endif /* ardour_generic_midi_control_protocol_h */
