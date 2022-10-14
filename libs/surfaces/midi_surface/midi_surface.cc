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

#include "pbd/debug.h"
#include "pbd/i18n.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/bundle.h"
#include "ardour/debug.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"

#include "midi_surface.h"

using namespace ARDOUR;
using namespace Glib;
using namespace PBD;

MIDISurface::MIDISurface (ARDOUR::Session& s, std::string const & namestr, bool use_pad_filter)
	: ControlProtocol (s, namestr)
	, AbstractUI<MidiSurfaceRequest> (namestr)
	, with_pad_filter (use_pad_filter)
	, _in_use (false)
{

	ARDOUR::AudioEngine::instance()->PortRegisteredOrUnregistered.connect (port_connections, MISSING_INVALIDATOR, boost::bind (&MIDISurface::port_registration_handler, this), this);
	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connections, MISSING_INVALIDATOR, boost::bind (&MIDISurface::connection_handler, this, _1, _2, _3, _4, _5), this);
	port_registration_handler ();
}

int
MIDISurface::ports_acquire ()
{
	DEBUG_TRACE (DEBUG::MIDISurface, "acquiring ports\n");

	/* setup ports */

	_async_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, X_("Push 2 in"), true);
	_async_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("Push 2 out"), true);

	if (_async_in == 0 || _async_out == 0) {
		DEBUG_TRACE (DEBUG::MIDISurface, "cannot register ports\n");
		return -1;
	}

	/* We do not add our ports to the input/output bundles because we don't
	 * want users wiring them by hand. They could use JACK tools if they
	 * really insist on that (and use JACK)
	 */

	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in).get();
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_out).get();

	/* Create a shadow port where, depending on the state of the surface,
	 * we will make pad note on/off events appear. The surface code will
	 * automatically this port to the first selected MIDI track.
	 */

	if (with_pad_filter) {
		boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in)->add_shadow_port (string_compose (_("%1 Pads"), X_("Push 2")), boost::bind (&MIDISurface::pad_filter, this, _1, _2));
		boost::shared_ptr<MidiPort> shadow_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in)->shadow_port();

		if (shadow_port) {

			_output_bundle.reset (new ARDOUR::Bundle (_("Push 2 Pads"), false));

			_output_bundle->add_channel (
				shadow_port->name(),
				ARDOUR::DataType::MIDI,
				session->engine().make_port_name_non_relative (shadow_port->name())
				);
		}
	}

	session->BundleAddedOrRemoved ();

	connect_to_parser ();

	/* Connect input port to event loop */

	AsyncMIDIPort* asp;

	asp = dynamic_cast<AsyncMIDIPort*> (_input_port);
	asp->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &MIDISurface::midi_input_handler), _input_port));
	asp->xthread().attach (main_loop()->get_context());

	return 0;
}

void
MIDISurface::ports_release ()
{
	DEBUG_TRACE (DEBUG::MIDISurface, "releasing ports\n");

	/* wait for button data to be flushed */
	AsyncMIDIPort* asp;
	asp = dynamic_cast<AsyncMIDIPort*> (_output_port);
	asp->drain (10000, 500000);

	{
		Glib::Threads::Mutex::Lock em (AudioEngine::instance()->process_lock());
		AudioEngine::instance()->unregister_port (_async_in);
		AudioEngine::instance()->unregister_port (_async_out);
	}

	_async_in.reset ((ARDOUR::Port*) 0);
	_async_out.reset ((ARDOUR::Port*) 0);
	_input_port = 0;
	_output_port = 0;
}

void
MIDISurface::port_registration_handler ()
{
	if (!_async_in || !_async_out) {
		/* ports not registered yet */
		return;
	}

	if (_async_in->connected() && _async_out->connected()) {
		/* don't waste cycles here */
		return;
	}

	std::vector<std::string> in;
	std::vector<std::string> out;

	AudioEngine::instance()->get_ports (string_compose (".*%1", input_port_name()), DataType::MIDI, PortFlags (IsPhysical|IsOutput), in);
	AudioEngine::instance()->get_ports (string_compose (".*%1", output_port_name()), DataType::MIDI, PortFlags (IsPhysical|IsInput), out);

	if (!in.empty() && !out.empty()) {
		if (!_async_in->connected()) {
			AudioEngine::instance()->connect (_async_in->name(), in.front());
		}
		if (!_async_out->connected()) {
			AudioEngine::instance()->connect (_async_out->name(), out.front());
		}
	}
}

bool
MIDISurface::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
{
	DEBUG_TRACE (DEBUG::MIDISurface, "FaderPort::connection_handler start\n");
	if (!_input_port || !_output_port) {
		return false;
	}

	std::string ni = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_async_in)->name());
	std::string no = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_async_out)->name());

	if (ni == name1 || ni == name2) {
		if (yn) {
			_connection_state |= InputConnected;
		} else {
			_connection_state &= ~InputConnected;
		}
	} else if (no == name1 || no == name2) {
		if (yn) {
			_connection_state |= OutputConnected;
		} else {
			_connection_state &= ~OutputConnected;
		}
	} else {
		DEBUG_TRACE (DEBUG::MIDISurface, string_compose ("Connections between %1 and %2 changed, but I ignored it\n", name1, name2));
		/* not our ports */
		return false;
	}

	DEBUG_TRACE (DEBUG::MIDISurface, string_compose ("our ports changed connection state: %1 -> %2 connected ? %3\n",
	                                           name1, name2, yn));

	if ((_connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {

		/* XXX this is a horrible hack. Without a short sleep here,
		   something prevents the device wakeup messages from being
		   sent and/or the responses from being received.
		*/

		g_usleep (100000);
                DEBUG_TRACE (DEBUG::MIDISurface, "device now connected for both input and output\n");

                /* may not have the device open if it was just plugged
                   in. Really need USB device detection rather than MIDI port
                   detection for this to work well.
                */

                device_acquire ();
                begin_using_device ();

	} else {
		DEBUG_TRACE (DEBUG::MIDISurface, "Device disconnected (input or output or both) or not yet fully connected\n");
		stop_using_device ();
	}

	ConnectionChange (); /* emit signal for our GUI */

	DEBUG_TRACE (DEBUG::MIDISurface, "connection_handler  end\n");

	return true; /* connection status changed */
}

boost::shared_ptr<Port>
MIDISurface::output_port()
{
	return _async_out;
}

boost::shared_ptr<Port>
MIDISurface::input_port()
{
	return _async_in;
}

void
MIDISurface::write (const MidiByteArray& data)
{
	/* immediate delivery */
	_output_port->write (&data[0], data.size(), 0);
}

bool
MIDISurface::midi_input_handler (IOCondition ioc, MIDI::Port* port)
{
	if (ioc & ~IO_IN) {
		DEBUG_TRACE (DEBUG::MIDISurface, "MIDI port closed\n");
		return false;
	}

	if (ioc & IO_IN) {

		DEBUG_TRACE (DEBUG::MIDISurface, string_compose ("something happened on  %1\n", port->name()));

		AsyncMIDIPort* asp = dynamic_cast<AsyncMIDIPort*>(port);
		if (asp) {
			asp->clear ();
		}

		DEBUG_TRACE (DEBUG::MIDISurface, string_compose ("data available on %1\n", port->name()));
		if (_in_use) {
			samplepos_t now = AudioEngine::instance()->sample_time();
			port->parse (now);
		}
	}

	return true;
}

void
MIDISurface::connect_to_parser ()
{
	DEBUG_TRACE (DEBUG::MIDISurface, string_compose ("Connecting to signals on port %2\n", _input_port->name()));

	MIDI::Parser* p = _input_port->parser();

	/* Incoming sysex */
	p->sysex.connect_same_thread (*this, boost::bind (&MIDISurface::handle_midi_sysex, this, _1, _2, _3));
	/* V-Pot messages are Controller */
	p->controller.connect_same_thread (*this, boost::bind (&MIDISurface::handle_midi_controller_message, this, _1, _2));
	/* Button messages are NoteOn */
	p->note_on.connect_same_thread (*this, boost::bind (&MIDISurface::handle_midi_note_on_message, this, _1, _2));
	/* Button messages are NoteOn but libmidi++ sends note-on w/velocity = 0 as note-off so catch them too */
	p->note_off.connect_same_thread (*this, boost::bind (&MIDISurface::handle_midi_note_on_message, this, _1, _2));
	/* Fader messages are Pitchbend */
	p->channel_pitchbend[0].connect_same_thread (*this, boost::bind (&MIDISurface::handle_midi_pitchbend_message, this, _1, _2));
}



void
MIDISurface::thread_init ()
{
	pthread_set_name (event_loop_name().c_str());

	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 128);

	set_thread_priority ();
}

void
MIDISurface::connect_session_signals()
{
	// receive routes added
	//session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_routes_added, this, _1), this);
	// receive VCAs added
	//session->vca_manager().VCAAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MIDISurface::notify_vca_added, this, _1), this);

	// receive record state toggled
	session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MIDISurface::notify_record_state_changed, this), this);
	// receive transport state changed
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MIDISurface::notify_transport_state_changed, this), this);
	session->TransportLooped.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&MIDISurface::notify_loop_state_changed, this), this);
	// receive punch-in and punch-out
	Config->ParameterChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MIDISurface::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&MIDISurface::notify_parameter_changed, this, _1), this);
	// receive rude solo changed
	session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MIDISurface::notify_solo_active_changed, this, _1), this);
}

XMLNode&
MIDISurface::get_state() const
{
	XMLNode& node (ControlProtocol::get_state());
	XMLNode* child;

	child = new XMLNode (X_("Input"));
	child->add_child_nocopy (_async_in->get_state());
	node.add_child_nocopy (*child);
	child = new XMLNode (X_("Output"));
	child->add_child_nocopy (_async_out->get_state());
	node.add_child_nocopy (*child);

	return node;
}

int
MIDISurface::set_state (const XMLNode & node, int version)
{
	DEBUG_TRACE (DEBUG::MIDISurface, string_compose ("MIDISurface::set_state: active %1\n", active()));

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	XMLNode* child;

	if ((child = node.child (X_("Input"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			_async_in->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			_async_out->set_state (*portnode, version);
		}
	}

	return 0;
}

void
MIDISurface::do_request (MidiSurfaceRequest * req)
{
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop_using_device ();
	}
}

int
MIDISurface::begin_using_device ()
{
	_in_use = true;
	return 0;
}

int
MIDISurface::stop_using_device ()
{
	_in_use = false;
	return 0;
}
