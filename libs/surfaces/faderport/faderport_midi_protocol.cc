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

#include "faderport_midi_protocol.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace std;

#include "i18n.h"

#define midi_ui_context() MidiControlUI::instance() /* a UICallback-derived object that specifies the event loop for signal handling */

FaderportMidiControlProtocol::FaderportMidiControlProtocol (Session& s)
	: ControlProtocol (s, _("Faderport"))
	, _motorised (true)
	, _threshold (10)
	, gui (0)
{
	_async_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, "Faderport Recv", true);
	_async_out = AudioEngine::instance()->register_output_port (DataType::MIDI, "Faderport Send", true);

	if (_async_in == 0 || _async_out == 0) {
		throw failed_constructor();
	}

	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in).get();
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_out).get();
		
	do_feedback = false;
	_feedback_interval = 10 * 1000; // microseconds
	last_feedback_time = 0;
	native_counter = 0;
	
	_current_bank = 0;
	_bank_size = 0;

//NOTE TO PAUL:
// "midi_receiver" and "midi_input_handler"
// were 2 different approaches to try to capture MIDI data;  neither seems to work as expected.


//not sure if this should do anything
	(*_input_port).parser()->any.connect_same_thread (midi_recv_connection, boost::bind (&FaderportMidiControlProtocol::midi_receiver, this, _1, _2, _3));

//this is raw port acces (?)
//	_input_port->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &FaderportMidiControlProtocol::midi_input_handler), _input_port));

	Session::SendFeedback.connect_same_thread (*this, boost::bind (&FaderportMidiControlProtocol::send_feedback, this));
	//Session::SendFeedback.connect (*this, MISSING_INVALIDATOR, boost::bind (&FaderportMidiControlProtocol::send_feedback, this), midi_ui_context());;

	/* this one is cross-thread */

	//Route::RemoteControlIDChange.connect (*this, MISSING_INVALIDATOR, boost::bind (&FaderportMidiControlProtocol::reset_controllables, this), midi_ui_context());

}

FaderportMidiControlProtocol::~FaderportMidiControlProtocol ()
{
	if (_input_port) {
		DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("unregistering input port %1\n", _async_in->name()));
		AudioEngine::instance()->unregister_port (_async_in);
		_async_in.reset ((ARDOUR::Port*) 0);
	}

	if (_output_port) {
//		_output_port->drain (10000);  //ToDo:  is this necessary?  It hangs the shutdown, for me
		DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("unregistering output port %1\n", _async_out->name()));
		AudioEngine::instance()->unregister_port (_async_out);
		_async_out.reset ((ARDOUR::Port*) 0);
	}

	tear_down_gui ();
}

void
FaderportMidiControlProtocol::midi_receiver (MIDI::Parser &p, MIDI::byte *, size_t)
{
//NOTE:  this never did anything
//	printf("got some midi\n");
}


int
FaderportMidiControlProtocol::set_active (bool /*yn*/)
{
	return 0;
}

void
FaderportMidiControlProtocol::set_feedback_interval (microseconds_t ms)
{
	_feedback_interval = ms;
}

void
FaderportMidiControlProtocol::send_feedback ()
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

	//occasionally tell the Faderport to go into "Native" mode
	//ToDo:  trigger this on MIDI port connection ?
	native_counter++;
	if (native_counter > 10) {
		native_counter = 0;
		MIDI::byte midibuf[64];
		MIDI::byte *buf = midibuf;
		*buf++ = (0x91);
		*buf++ = (0x00);
		*buf++ = (0x64);
		_output_port->write (buf, 3, 0);
	}
	
	last_feedback_time = now;
}

bool
FaderportMidiControlProtocol::midi_input_handler (Glib::IOCondition ioc, ARDOUR::AsyncMIDIPort* port)
{
	DEBUG_TRACE (DEBUG::MidiIO, string_compose ("something happend on  %1\n", ((ARDOUR::Port*)port)->name()));

	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		AsyncMIDIPort* asp = dynamic_cast<AsyncMIDIPort*> (port);
		if (asp) {
			asp->clear ();
		}

		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("data available on %1\n", ((ARDOUR::Port*)port)->name()));
//		framepos_t now = _session.engine().sample_time();
//		port->parse (now);
	}

	return true;
}


XMLNode&
FaderportMidiControlProtocol::get_state ()
{
	XMLNode& node (ControlProtocol::get_state());
	char buf[32];

	return node;
}

int
FaderportMidiControlProtocol::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	const XMLProperty* prop;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	return 0;
}

int
FaderportMidiControlProtocol::set_feedback (bool yn)
{
	do_feedback = yn;
	last_feedback_time = 0;
	return 0;
}

bool
FaderportMidiControlProtocol::get_feedback () const
{
	return do_feedback;
}

void
FaderportMidiControlProtocol::set_current_bank (uint32_t b)
{
	_current_bank = b;
//	reset_controllables ();
}

void
FaderportMidiControlProtocol::next_bank ()
{
	_current_bank++;
//	reset_controllables ();
}

void
FaderportMidiControlProtocol::prev_bank()
{
	if (_current_bank) {
		_current_bank--;
//		reset_controllables ();
	}
}

void
FaderportMidiControlProtocol::set_motorised (bool m)
{
	_motorised = m;
}

void
FaderportMidiControlProtocol::set_threshold (int t)
{
	_threshold = t;
}
