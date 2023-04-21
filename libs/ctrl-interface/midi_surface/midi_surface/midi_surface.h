/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "control_protocol/control_protocol.h"
#include "midi++/types.h"

#include "midi_byte_array.h"

namespace MIDI {
	class Parser;
	class Port;
}

namespace ARDOUR {
	class Bundle;
	class Port;
	class MidiBuffer;
}

struct MidiSurfaceRequest : public BaseUI::BaseRequestObject {
  public:
	MidiSurfaceRequest () {}
	~MidiSurfaceRequest () {}
};

class MIDISurface : public ARDOUR::ControlProtocol
                  , public AbstractUI<MidiSurfaceRequest>
{
  public:
	MIDISurface (ARDOUR::Session&, std::string const & name, std::string const & port_name_prefix, bool use_pad_filter);
	~MIDISurface ();

	std::shared_ptr<ARDOUR::Port> input_port();
	std::shared_ptr<ARDOUR::Port> output_port();

	// Bundle to represent our input ports
	std::shared_ptr<ARDOUR::Bundle> _input_bundle;
	// Bundle to represent our output ports
	std::shared_ptr<ARDOUR::Bundle> _output_bundle;

	ARDOUR::Session & get_session() { return *session; }

	virtual std::string input_port_name () const = 0;
	virtual std::string output_port_name () const = 0;

	void write (const MidiByteArray&);
	void write (MIDI::byte const *, size_t);

	XMLNode& get_state() const;
	int set_state (const XMLNode & node, int version);

	std::list<std::shared_ptr<ARDOUR::Bundle> > bundles ();

	PBD::Signal0<void> ConnectionChange;

	CONTROL_PROTOCOL_THREADS_NEED_TEMPO_MAP_DECL();

  protected:
	bool with_pad_filter;
	bool _in_use;
	std::string port_name_prefix;
	MIDI::Port* _input_port;
	MIDI::Port* _output_port;

	std::shared_ptr<ARDOUR::Port> _async_in;
	std::shared_ptr<ARDOUR::Port> _async_out;

	void do_request (MidiSurfaceRequest*);

	virtual void connect_to_parser ();
	virtual void handle_midi_pitchbend_message (MIDI::Parser&, MIDI::pitchbend_t) {}
	virtual void handle_midi_polypressure_message (MIDI::Parser&, MIDI::EventTwoBytes*) {}
	virtual void handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes*) {}
	virtual void handle_midi_note_on_message (MIDI::Parser&, MIDI::EventTwoBytes*) {}
	virtual void handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes*) {}
	virtual void handle_midi_sysex (MIDI::Parser&, MIDI::byte *, size_t) {}

	virtual bool midi_input_handler (Glib::IOCondition ioc, MIDI::Port* port);

	virtual void thread_init ();

	PBD::ScopedConnectionList session_connections;

	virtual void connect_session_signals ();
	virtual void notify_record_state_changed () {}
	virtual void notify_transport_state_changed () {}
	virtual void notify_loop_state_changed () {}
	virtual void notify_parameter_changed (std::string) {}
	virtual void notify_solo_active_changed (bool) {}

	virtual void port_registration_handler ();
	virtual bool pad_filter (ARDOUR::MidiBuffer& in, ARDOUR::MidiBuffer& out) const { return false; }

	enum ConnectionState {
		InputConnected = 0x1,
		OutputConnected = 0x2
	};

	int _connection_state;

	PBD::ScopedConnectionList port_connections;

	virtual int ports_acquire ();
	virtual void ports_release ();

	virtual int begin_using_device ();
	virtual int stop_using_device ();
	virtual int device_acquire () = 0;
	virtual void device_release () = 0;

	void drop ();
	void port_setup ();

  private:
	bool connection_handler (std::weak_ptr<ARDOUR::Port>, std::string name1, std::weak_ptr<ARDOUR::Port>, std::string name2, bool yn);
};
