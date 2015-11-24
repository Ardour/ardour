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

#include <stdint.h>

#include <sstream>
#include <algorithm>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/controllable_descriptor.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/xml++.h"
#include "pbd/compose.h"

#include "midi++/port.h"

#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/midi_ui.h"
#include "ardour/midi_port.h"
#include "ardour/rc_configuration.h"
#include "ardour/midiport_manager.h"
#include "ardour/debug.h"
#include "ardour/async_midi_port.h"

#include "faderport.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace std;

#include "i18n.h"

#define midi_ui_context() MidiControlUI::instance() /* a UICallback-derived object that specifies the event loop for signal handling */

FaderPort::FaderPort (Session& s)
	: ControlProtocol (s, _("Faderport"))
	, _motorised (true)
	, _threshold (10)
	, gui (0)
	, connection_state (ConnectionState (0))
	, _device_active (false)
	, fader_msb (0)
	, fader_lsb (0)
{
	boost::shared_ptr<ARDOUR::Port> inp;
	boost::shared_ptr<ARDOUR::Port> outp;

	inp  = AudioEngine::instance()->register_input_port (DataType::MIDI, "Faderport Recv", true);
	outp = AudioEngine::instance()->register_output_port (DataType::MIDI, "Faderport Send", true);

	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(inp);
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(outp);

	if (_input_port == 0 || _output_port == 0) {
		throw failed_constructor();
	}

	do_feedback = false;
	_feedback_interval = 10 * 1000; // microseconds
	last_feedback_time = 0;
	native_counter = 0;

	_current_bank = 0;
	_bank_size = 0;

	/* handle device inquiry response */
	_input_port->parser()->sysex.connect_same_thread (midi_connections, boost::bind (&FaderPort::sysex_handler, this, _1, _2, _3));
	/* handle switches */
	_input_port->parser()->poly_pressure.connect_same_thread (midi_connections, boost::bind (&FaderPort::switch_handler, this, _1, _2));
	/* handle encoder */
	_input_port->parser()->pitchbend.connect_same_thread (midi_connections, boost::bind (&FaderPort::encoder_handler, this, _1, _2));
	/* handle fader */
	_input_port->parser()->controller.connect_same_thread (midi_connections, boost::bind (&FaderPort::fader_handler, this, _1, _2));

	/* This connection means that whenever data is ready from the input
	 * port, the relevant thread will invoke our ::midi_input_handler()
	 * method, which will read the data, and invoke the parser.
	 */

	_input_port->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &FaderPort::midi_input_handler), _input_port));
	_input_port->xthread().attach (midi_ui_context()->main_loop()->get_context());

	Session::SendFeedback.connect_same_thread (*this, boost::bind (&FaderPort::send_feedback, this));
	//Session::SendFeedback.connect (*this, MISSING_INVALIDATOR, boost::bind (&FaderPort::send_feedback, this), midi_ui_context());;

	/* this one is cross-thread */

	Route::RemoteControlIDChange.connect (*this, MISSING_INVALIDATOR, boost::bind (&FaderPort::reset_controllables, this), midi_ui_context());

	/* Catch port connections and disconnections */
	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR, boost::bind (&FaderPort::connection_handler, this, _1, _2, _3, _4, _5), midi_ui_context());

	buttons.insert (std::make_pair (18, ButtonID (_("Mute"), 18, 21)));
	buttons.insert (std::make_pair (17, ButtonID (_("Solo"), 17, 22)));
	buttons.insert (std::make_pair (16, ButtonID (_("Rec"), 16, 23)));
	buttons.insert (std::make_pair (19, ButtonID (_("Left"), 19, 20)));
	buttons.insert (std::make_pair (20, ButtonID (_("Bank"), 20, 19)));
	buttons.insert (std::make_pair (21, ButtonID (_("Right"), 21, 18)));
	buttons.insert (std::make_pair (22, ButtonID (_("Output"), 22, 17)));
	buttons.insert (std::make_pair (10, ButtonID (_("Read"), 10, 13)));
	buttons.insert (std::make_pair (9, ButtonID (_("Write"), 9, 14)));
	buttons.insert (std::make_pair (8, ButtonID (_("Touch"), 8, 15)));
	buttons.insert (std::make_pair (23, ButtonID (_("Off"), 23, 16)));
	buttons.insert (std::make_pair (11, ButtonID (_("Mix"), 11, 12)));
	buttons.insert (std::make_pair (12, ButtonID (_("Proj"), 12, 11)));
	buttons.insert (std::make_pair (13, ButtonID (_("Trns"), 13, 10)));
	buttons.insert (std::make_pair (14, ButtonID (_("Undo"), 14, 9)));
	buttons.insert (std::make_pair (2, ButtonID (_("Shift"), 2, 5)));
	buttons.insert (std::make_pair (1, ButtonID (_("Punch"), 1, 6)));
	buttons.insert (std::make_pair (0, ButtonID (_("User"), 0, 7)));
	buttons.insert (std::make_pair (15, ButtonID (_("Loop"), 15, 8)));
	buttons.insert (std::make_pair (3, ButtonID (_("Rewind"), 3, 4)));
	buttons.insert (std::make_pair (4, ButtonID (_("Ffwd"), 4, 3)));
	buttons.insert (std::make_pair (5, ButtonID (_("Stop"), 5, 2)));
	buttons.insert (std::make_pair (6, ButtonID (_("Play"), 6, 1)));
	buttons.insert (std::make_pair (7, ButtonID (_("RecEnable"), 7, 0)));
	buttons.insert (std::make_pair (127, ButtonID (_("Fader (touch)"), 127, -1)));
}

FaderPort::~FaderPort ()
{
	if (_input_port) {
		DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("unregistering input port %1\n", boost::shared_ptr<ARDOUR::Port>(_input_port)->name()));
		AudioEngine::instance()->unregister_port (_input_port);
		_input_port.reset ();
	}

	if (_output_port) {
//		_output_port->drain (10000);  //ToDo:  is this necessary?  It hangs the shutdown, for me
		DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("unregistering output port %1\n", boost::shared_ptr<ARDOUR::Port>(_output_port)->name()));
		AudioEngine::instance()->unregister_port (_output_port);
		_output_port.reset ();
	}

	tear_down_gui ();
}

void
FaderPort::switch_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	map<int,ButtonID>::const_iterator b = buttons.find (tb->controller_number);

	if (b != buttons.end()) {

		cerr << b->second.name << endl;

		if (b->second.out >= 0) {
			/* send feedback to turn on the LED */

			MIDI::byte buf[3];
			buf[0] = 0xa0;
			buf[1] = b->second.out;
			buf[2] = tb->value;

			_output_port->write (buf, 3, 0);
		}
	}
}

void
FaderPort::encoder_handler (MIDI::Parser &, MIDI::pitchbend_t pb)
{
	if (pb < 8192) {
		cerr << "Encoder right\n";
	} else {
		cerr << "Encoder left\n";
	}
}

void
FaderPort::fader_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb)
{
	bool was_fader = false;

	if (tb->controller_number == 0x0) {
		fader_msb = tb->value;
		was_fader = true;
	} else if (tb->controller_number == 0x20) {
		fader_lsb = tb->value;
		was_fader = true;
	}

	if (was_fader) {
		cerr << "Fader now at " << ((fader_msb<<7)|fader_lsb) << endl;
	}
}

void
FaderPort::sysex_handler (MIDI::Parser &p, MIDI::byte *buf, size_t sz)
{
	if (sz < 17) {
		return;
	}

	if (buf[2] == 0x7f &&
	    buf[3] == 0x06 &&
	    buf[4] == 0x02 &&
	    buf[5] == 0x0 &&
	    buf[6] == 0x1 &&
	    buf[7] == 0x06 &&
	    buf[8] == 0x02 &&
	    buf[9] == 0x0 &&
	    buf[10] == 0x01 &&
	    buf[11] == 0x0) {
		_device_active = true;

		cerr << "FaderPort identified\n";

		/* put it into native mode */

		MIDI::byte native[3];
		native[0] = 0x91;
		native[1] = 0x00;
		native[2] = 0x64;

		_output_port->write (native, 3, 0);
	}
}

int
FaderPort::set_active (bool /*yn*/)
{
	return 0;
}

void
FaderPort::set_feedback_interval (microseconds_t ms)
{
	_feedback_interval = ms;
}

void
FaderPort::send_feedback ()
{
	/* This is executed in RT "process" context", so no blocking calls
	 */

	if (!do_feedback) {
		return;
	}

	microseconds_t now = get_microseconds ();

	if (last_feedback_time != 0) {
		if ((now - last_feedback_time) < _feedback_interval) {
			return;
		}
	}

	last_feedback_time = now;
}

bool
FaderPort::midi_input_handler (Glib::IOCondition ioc, boost::shared_ptr<ARDOUR::AsyncMIDIPort> port)
{
	DEBUG_TRACE (DEBUG::MidiIO, string_compose ("something happend on  %1\n", boost::shared_ptr<MIDI::Port>(port)->name()));

	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		if (port) {
			port->clear ();
		}

		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("data available on %1\n", boost::shared_ptr<MIDI::Port>(port)->name()));
		framepos_t now = session->engine().sample_time();
		port->parse (now);
	}

	return true;
}


XMLNode&
FaderPort::get_state ()
{
	XMLNode& node (ControlProtocol::get_state());

	XMLNode* child;

	child = new XMLNode (X_("Input"));
	child->add_child_nocopy (boost::shared_ptr<ARDOUR::Port>(_input_port)->get_state());
	node.add_child_nocopy (*child);


	child = new XMLNode (X_("Output"));
	child->add_child_nocopy (boost::shared_ptr<ARDOUR::Port>(_output_port)->get_state());
	node.add_child_nocopy (*child);

	return node;
}

int
FaderPort::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode const* child;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	if ((child = node.child (X_("Input"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			boost::shared_ptr<ARDOUR::Port>(_input_port)->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			boost::shared_ptr<ARDOUR::Port>(_output_port)->set_state (*portnode, version);
		}
	}

	return 0;
}

int
FaderPort::set_feedback (bool yn)
{
	do_feedback = yn;
	last_feedback_time = 0;
	return 0;
}

bool
FaderPort::get_feedback () const
{
	return do_feedback;
}

void
FaderPort::set_current_bank (uint32_t b)
{
	_current_bank = b;
//	reset_controllables ();
}

void
FaderPort::next_bank ()
{
	_current_bank++;
//	reset_controllables ();
}

void
FaderPort::prev_bank()
{
	if (_current_bank) {
		_current_bank--;
//		reset_controllables ();
	}
}

void
FaderPort::set_motorised (bool m)
{
	_motorised = m;
}

void
FaderPort::set_threshold (int t)
{
	_threshold = t;
}

void
FaderPort::reset_controllables ()
{
}

bool
FaderPort::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
{
	if (!_input_port || !_output_port) {
		return false;
	}

	string ni = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_input_port)->name());
	string no = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_output_port)->name());

	if (ni == name1 || ni == name2) {
		if (yn) {
			connection_state |= InputConnected;
		} else {
			connection_state &= ~InputConnected;
		}
	} else if (no == name1 || no == name2) {
		if (yn) {
			connection_state |= OutputConnected;
		} else {
			connection_state &= ~OutputConnected;
		}
	} else {
		/* not our ports */
		return false;
	}

	if ((connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {

		/* XXX this is a horrible hack. Without a short sleep here,
		   something prevents the device wakeup messages from being
		   sent and/or the responses from being received.
		*/

		g_usleep (100000);
		connected ();

	} else {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Surface %1 disconnected (input or output or both)\n", _name));
		_device_active = false;
	}

	return true; /* connection status changed */
}

void
FaderPort::connected ()
{
	std::cerr << "faderport connected\n";

	/* send device inquiry */

	MIDI::byte buf[6];

	buf[0] = 0xf0;
	buf[1] = 0x7e;
	buf[2] = 0x7f;
	buf[3] = 0x06;
	buf[4] = 0x01;
	buf[5] = 0xf7;

	_output_port->write (buf, 6, 0);
}
