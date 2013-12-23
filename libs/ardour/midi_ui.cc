/*
  Copyright (C) 2009 Paul Davis

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

#include "i18n.h"

MidiControlUI* MidiControlUI::_instance = 0;

#include "pbd/abstract_ui.cc"  /* instantiate the template */

MidiControlUI::MidiControlUI (Session& s)
	: AbstractUI<MidiUIRequest> (X_("midiui"))
	, _session (s)
{
	_instance = this;
}

MidiControlUI::~MidiControlUI ()
{
	clear_ports ();
	_instance = 0;
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
MidiControlUI::midi_input_handler (IOCondition ioc, AsyncMIDIPort* port)
{
	DEBUG_TRACE (DEBUG::MidiIO, string_compose ("something happend on  %1\n", ((ARDOUR::Port*)port)->name()));

	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		CrossThreadChannel::drain (port->selectable());

		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("data available on %1\n", ((ARDOUR::Port*)port)->name()));
		framepos_t now = _session.engine().sample_time();
		port->parse (now);
	}

	return true;
}

void
MidiControlUI::clear_ports ()
{
	for (PortSources::iterator i = port_sources.begin(); i != port_sources.end(); ++i) {
		g_source_destroy (*i);
		g_source_unref (*i);
	}

	port_sources.clear ();
}

void
MidiControlUI::reset_ports ()
{
	if (!port_sources.empty()) {
		return;
	}
	
	vector<AsyncMIDIPort*> ports;
	AsyncMIDIPort* p;
	
	if ((p = dynamic_cast<AsyncMIDIPort*> (_session.midi_input_port()))) {
		ports.push_back (p);
	}
	
	
	if ((p = dynamic_cast<AsyncMIDIPort*> (_session.mmc_input_port()))) {
		ports.push_back (p);
	}
	
	if (ports.empty()) {
		return;
	}
	
	int fd;
	for (vector<AsyncMIDIPort*>::const_iterator pi = ports.begin(); pi != ports.end(); ++pi) {

		if ((fd = (*pi)->selectable ()) >= 0) {
			Glib::RefPtr<IOSource> psrc = IOSource::create (fd, IO_IN|IO_HUP|IO_ERR);
			
			psrc->connect (sigc::bind (sigc::mem_fun (this, &MidiControlUI::midi_input_handler), *pi));
			psrc->attach (_main_loop->get_context());
			
			// glibmm hack: for now, store only the GSource*
			
			port_sources.push_back (psrc->gobj());
			g_source_ref (psrc->gobj());
		}
	}
}

void
MidiControlUI::thread_init ()
{
	struct sched_param rtparam;

	pthread_set_name (X_("midiUI"));

	PBD::notify_gui_about_thread_creation (X_("gui"), pthread_self(), X_("MIDI"), 2048);
	SessionEvent::create_per_thread_pool (X_("MIDI I/O"), 128);

	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 9; /* XXX should be relative to audio (JACK) thread */

	if (pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam) != 0) {
		// do we care? not particularly.
	}

	reset_ports ();
}

