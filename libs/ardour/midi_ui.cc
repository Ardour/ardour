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

#include "pbd/pthread_utils.h"

#include "midi++/manager.h"
#include "midi++/port.h"

#include "ardour/debug.h"
#include "ardour/audioengine.h"
#include "ardour/midi_ui.h"
#include "ardour/session.h"
#include "ardour/session_event.h"
#include "ardour/types.h"

using namespace std;
using namespace ARDOUR;
using namespace Glib;

#include "i18n.h"

BaseUI::RequestType MidiControlUI::PortChange = BaseUI::new_request_type();

#include "pbd/abstract_ui.cc"  /* instantiate the template */

MidiControlUI::MidiControlUI (Session& s)
	: AbstractUI<MidiUIRequest> (_("midiui"))
	, _session (s) 
{
	MIDI::Manager::instance()->PortsChanged.connect (mem_fun (*this, &MidiControlUI::change_midi_ports));
}

MidiControlUI::~MidiControlUI ()
{
	clear_ports ();
}

void
MidiControlUI::do_request (MidiUIRequest* req)
{
	if (req->type == PortChange) {

		/* restart event loop with new ports */
		DEBUG_TRACE (DEBUG::MidiIO, "reset ports\n");
		reset_ports ();

	} else if (req->type == CallSlot) {

		req->the_slot ();
	}
}

void
MidiControlUI::change_midi_ports ()
{
	MidiUIRequest* req = get_request (PortChange);
	if (req == 0) {
		return;
	}
	send_request (req);
}

bool
MidiControlUI::midi_input_handler (IOCondition ioc, MIDI::Port* port)
{
	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		if (port->must_drain_selectable()) {
			CrossThreadChannel::drain (port->selectable());
		}

		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("data available on %1\n", port->name()));
		nframes64_t now = _session.engine().frame_time();
		port->parse (now);
	}

	return true;
}

void
MidiControlUI::clear_ports ()
{
	for (PortSources::iterator i = port_sources.begin(); i != port_sources.end(); ++i) {
		/* remove existing sources from the event loop */
		(*i)->destroy ();
	}

	port_sources.clear ();
}

void
MidiControlUI::reset_ports ()
{
	clear_ports ();

	MIDI::Manager::PortList plist = MIDI::Manager::instance()->get_midi_ports ();

	for (MIDI::Manager::PortList::iterator i = plist.begin(); i != plist.end(); ++i) {
		int fd;
		if ((fd = (*i)->selectable ()) >= 0) {
			Glib::RefPtr<IOSource> psrc = IOSource::create (fd, IO_IN|IO_HUP|IO_ERR);
			psrc->connect (bind (mem_fun (*this, &MidiControlUI::midi_input_handler), (*i)));
			port_sources.push_back (psrc);
		} 
	}

	for (PortSources::iterator i = port_sources.begin(); i != port_sources.end(); ++i) {
		(*i)->attach (_main_loop->get_context());
	}
}

void
MidiControlUI::thread_init ()
{	
	struct sched_param rtparam;

	PBD::notify_gui_about_thread_creation (X_("gui"), pthread_self(), X_("MIDI"), 2048);
	SessionEvent::create_per_thread_pool (X_("MIDI I/O"), 128);

	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 9; /* XXX should be relative to audio (JACK) thread */

	if (pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam) != 0) {
		// do we care? not particularly.
	}

	reset_ports ();
}

