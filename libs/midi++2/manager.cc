/*
    Copyright (C) 1998-99 Paul Barton-Davis 
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

    $Id$
*/

#include <fcntl.h>

#include <glib.h>

#include "pbd/error.h"

#include "midi++/types.h"
#include "midi++/manager.h"
#include "midi++/channel.h"
#include "midi++/port.h"
#include "midi++/mmc.h"

using namespace std;
using namespace MIDI;
using namespace PBD;

Manager *Manager::theManager = 0;

Manager::Manager (jack_client_t* jack) 
{
	_mmc = new MachineControl (this, jack);
	
	_mtc_input_port = add_port (new MIDI::Port ("MTC in", Port::IsInput, jack));
	_mtc_output_port = add_port (new MIDI::Port ("MTC out", Port::IsOutput, jack));
	_midi_input_port = add_port (new MIDI::Port ("MIDI control in", Port::IsInput, jack));
	_midi_output_port = add_port (new MIDI::Port ("MIDI control out", Port::IsOutput, jack));
	_midi_clock_input_port = add_port (new MIDI::Port ("MIDI clock in", Port::IsInput, jack));
	_midi_clock_output_port = add_port (new MIDI::Port ("MIDI clock out", Port::IsOutput, jack));
}

Manager::~Manager ()
{
	delete _mmc;
	
	/* This will delete our MTC etc. ports */
	for (PortList::iterator p = _ports.begin(); p != _ports.end(); ++p) {
		delete *p;
	}

	if (theManager == this) {
		theManager = 0;
	}
}

Port *
Manager::add_port (Port* p)
{
	_ports.push_back (p);

	PortsChanged (); /* EMIT SIGNAL */

	return p;
}

void
Manager::cycle_start (pframes_t nframes)
{
	for (PortList::iterator p = _ports.begin(); p != _ports.end(); ++p) {
		(*p)->cycle_start (nframes);
	}
}

void
Manager::cycle_end()
{
	for (PortList::iterator p = _ports.begin(); p != _ports.end(); ++p) {
		(*p)->cycle_end ();
	}
}

/** Re-register ports that disappear on JACK shutdown */
void
Manager::reestablish (jack_client_t* jack)
{
	for (PortList::const_iterator p = _ports.begin(); p != _ports.end(); ++p) {
		(*p)->reestablish (jack);
	}
}

/** Re-connect ports after a reestablish () */
void
Manager::reconnect ()
{
	for (PortList::const_iterator p = _ports.begin(); p != _ports.end(); ++p) {
		(*p)->reconnect ();
	}
}

Port*
Manager::port (string const & n)
{
	PortList::const_iterator p = _ports.begin();
	while (p != _ports.end() && (*p)->name() != n) {
		++p;
	}

	if (p == _ports.end()) {
		return 0;
	}

	return *p;
}

void
Manager::create (jack_client_t* jack)
{
	assert (theManager == 0);
	theManager = new Manager (jack);
}

void
Manager::set_port_states (list<XMLNode*> s)
{
	for (list<XMLNode*>::iterator i = s.begin(); i != s.end(); ++i) {
		for (PortList::const_iterator j = _ports.begin(); j != _ports.end(); ++j) {
			(*j)->set_state (**i);
		}
	}
}
