/*
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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
#include <cstdlib>

#include <sigc++/signal.h>

#include "pbd/pthread_utils.h"

#include "ardour/async_midi_port.h"
#include "ardour/debug.h"
#include "ardour/audioengine.h"
#include "ardour/midi_port.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_ui.h"
#include "ardour/session.h"
#include "ardour/session_event.h"
#include "ardour/types.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Glib;

#include "pbd/i18n.h"

MidiControlUI* MidiControlUI::_instance = 0;

#include "pbd/abstract_ui.cc"  /* instantiate the template */

MidiControlUI::MidiControlUI (Session& s)
	: AbstractUI<MidiUIRequest> (X_("midiUI"))
	, _session (s)
{
	_instance = this;
}

MidiControlUI::~MidiControlUI ()
{
	/* stop the thread */
	quit ();
	/* drop all ports as GIO::Sources */
	clear_ports ();
	/* we no longer exist */
	_instance = 0;
}

void*
MidiControlUI::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	   instantiated in this source module. To provide something visible for
	   use when registering the factory, we have this static method that is
	   template-free.
	*/
	return request_buffer_factory (num_requests);
}

void
MidiControlUI::do_request (MidiUIRequest* req)
{
	if (req->type == Quit) {
		BaseUI::quit ();
	} else if (req->type == CallSlot) {
		req->the_slot ();
	}
}

bool
MidiControlUI::midi_input_handler (IOCondition ioc, boost::weak_ptr<AsyncMIDIPort> wport)
{
	boost::shared_ptr<AsyncMIDIPort> port = wport.lock ();
	if (!port) {
		return false;
	}

	DEBUG_TRACE (DEBUG::MidiIO, string_compose ("something happend on  %1\n", boost::shared_ptr<ARDOUR::Port> (port)->name()));

	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		port->clear ();
		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("data available on %1\n", boost::shared_ptr<ARDOUR::Port>(port)->name()));
		samplepos_t now = _session.engine().sample_time();
		port->parse (now);
	}

	return true;
}

void
MidiControlUI::clear_ports ()
{
}

void
MidiControlUI::reset_ports ()
{
	vector<boost::shared_ptr<AsyncMIDIPort> > ports;
	boost::shared_ptr<AsyncMIDIPort> p;

	if ((p = boost::dynamic_pointer_cast<AsyncMIDIPort> (_session.mmc_input_port()))) {
		ports.push_back (p);
	}

	if ((p = boost::dynamic_pointer_cast<AsyncMIDIPort> (_session.scene_input_port()))) {
		ports.push_back (p);
	}

	if (ports.empty()) {
		return;
	}

	for (vector<boost::shared_ptr<AsyncMIDIPort> >::const_iterator pi = ports.begin(); pi != ports.end(); ++pi) {
		(*pi)->xthread().set_receive_handler (sigc::bind (
					sigc::mem_fun (this, &MidiControlUI::midi_input_handler), boost::weak_ptr<AsyncMIDIPort>(*pi)));
		(*pi)->xthread().attach (_main_loop->get_context());
	}
}

void
MidiControlUI::thread_init ()
{
	pthread_set_name (X_("midiUI"));

	PBD::notify_event_loops_about_thread_creation (pthread_self(), X_("midiUI"), 2048);
	SessionEvent::create_per_thread_pool (X_("midiUI"), 128);

	set_thread_priority ();

	reset_ports ();
}

