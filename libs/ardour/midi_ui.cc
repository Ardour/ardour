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
using namespace PBD;
using namespace Glib;

#include "i18n.h"

BaseUI::RequestType MidiControlUI::PortChange = BaseUI::new_request_type();
MidiControlUI* MidiControlUI::_instance = 0;

#include "pbd/abstract_ui.cc"  /* instantiate the template */

MidiControlUI::MidiControlUI (Session& s)
	: AbstractUI<MidiUIRequest> (X_("midiui"))
	, _session (s)
{
	MIDI::Manager::instance()->PortsChanged.connect_same_thread (rebind_connection, boost::bind (&MidiControlUI::change_midi_ports, this));
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
	if (req->type == PortChange) {

		/* restart event loop with new ports */
		DEBUG_TRACE (DEBUG::MidiIO, "reset ports\n");
		reset_ports ();

	} else if (req->type == CallSlot) {

#ifndef NDEBUG
		if (getenv ("DEBUG_THREADED_SIGNALS")) {
			cerr << "MIDI UI calls a slot\n";
		}
#endif

		req->the_slot ();

	} else if (req->type == Quit) {

		BaseUI::quit ();
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
	DEBUG_TRACE (DEBUG::MidiIO, string_compose ("something happend on  %1\n", port->name()));

	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {

		CrossThreadChannel::drain (port->selectable());

		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("data available on %1\n", port->name()));
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
	clear_ports ();

	boost::shared_ptr<const MIDI::Manager::PortList> plist = MIDI::Manager::instance()->get_midi_ports ();

	for (MIDI::Manager::PortList::const_iterator i = plist->begin(); i != plist->end(); ++i) {

		if (!(*i)->centrally_parsed()) {
			continue;
		}

		int fd;

		if ((fd = (*i)->selectable ()) >= 0) {
			Glib::RefPtr<IOSource> psrc = IOSource::create (fd, IO_IN|IO_HUP|IO_ERR);

			psrc->connect (sigc::bind (sigc::mem_fun (this, &MidiControlUI::midi_input_handler), *i));
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

