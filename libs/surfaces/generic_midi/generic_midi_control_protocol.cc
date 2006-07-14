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

    $Id$
*/

#include <algorithm>

#include <pbd/error.h>
#include <pbd/failed_constructor.h>

#include <midi++/port.h>
#include <midi++/manager.h>
#include <midi++/port_request.h>

#include <ardour/route.h>
#include <ardour/session.h>

#include "generic_midi_control_protocol.h"
#include "midicontrollable.h"

using namespace ARDOUR;
using namespace PBD;

#include "i18n.h"

GenericMidiControlProtocol::GenericMidiControlProtocol (Session& s)
	: ControlProtocol  (s, _("GenericMIDI"))
{
	MIDI::Manager* mm = MIDI::Manager::instance();

	/* XXX it might be nice to run "control" through i18n, but thats a bit tricky because
	   the name is defined in ardour.rc which is likely not internationalized.
	*/
	
	_port = mm->port (X_("control"));

	if (_port == 0) {
		error << _("no MIDI port named \"control\" exists - generic MIDI control disabled") << endmsg;
		throw failed_constructor();
	}

	_feedback_interval = 10000; // microseconds
	last_feedback_time = 0;

	Controllable::StartLearning.connect (mem_fun (*this, &GenericMidiControlProtocol::start_learning));
	Controllable::StopLearning.connect (mem_fun (*this, &GenericMidiControlProtocol::stop_learning));
	Session::SendFeedback.connect (mem_fun (*this, &GenericMidiControlProtocol::send_feedback));
}

GenericMidiControlProtocol::~GenericMidiControlProtocol ()
{
}

int
GenericMidiControlProtocol::set_active (bool yn)
{
	/* start/stop delivery/outbound thread */
	return 0;
}

void
GenericMidiControlProtocol::set_feedback_interval (microseconds_t ms)
{
	_feedback_interval = ms;
}

void 
GenericMidiControlProtocol::send_feedback ()
{
	microseconds_t now = get_microseconds ();

	if (last_feedback_time != 0) {
		if ((now - last_feedback_time) < _feedback_interval) {
			return;
		}
	}

	_send_feedback ();
	
	last_feedback_time = now;
}

void 
GenericMidiControlProtocol::_send_feedback ()
{
	const int32_t bufsize = 16 * 1024;
	MIDI::byte buf[bufsize];
	int32_t bsize = bufsize;
	MIDI::byte* end = buf;
	
	for (MIDIControllables::iterator r = controllables.begin(); r != controllables.end(); ++r) {
		end = (*r)->write_feedback (end, bsize);
	}
	
	if (end == buf) {
		return;
	} 

	// FIXME
	//_port->write (buf, (int32_t) (end - buf));
}

bool
GenericMidiControlProtocol::start_learning (Controllable* c)
{
	if (c == 0) {
		return false;
	}

	MIDIControllable* mc = new MIDIControllable (*_port, *c);
	
	{
		Glib::Mutex::Lock lm (pending_lock);
		std::pair<MIDIControllables::iterator,bool> result;
		result = pending_controllables.insert (mc);
		if (result.second) {
			c->LearningFinished.connect (bind (mem_fun (*this, &GenericMidiControlProtocol::learning_stopped), mc));
		}
	}

	mc->learn_about_external_control ();
	return true;
}

void
GenericMidiControlProtocol::learning_stopped (MIDIControllable* mc)
{
	Glib::Mutex::Lock lm (pending_lock);
	Glib::Mutex::Lock lm2 (controllables_lock);
	
	MIDIControllables::iterator i = find (pending_controllables.begin(), pending_controllables.end(), mc);

	if (i != pending_controllables.end()) {
		pending_controllables.erase (i);
	}

	controllables.insert (mc);
}

void
GenericMidiControlProtocol::stop_learning (Controllable* c)
{
	Glib::Mutex::Lock lm (pending_lock);

	/* learning timed out, and we've been told to consider this attempt to learn to be cancelled. find the
	   relevant MIDIControllable and remove it from the pending list.
	*/

	for (MIDIControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ++i) {
		if (&(*i)->get_controllable() == c) {
			(*i)->stop_learning ();
			delete (*i);
			pending_controllables.erase (i);
			break;
		}
	}
}

XMLNode&
GenericMidiControlProtocol::get_state () 
{
	XMLNode* node = new XMLNode (_name); /* node name must match protocol name */
	XMLNode* children = new XMLNode (X_("controls"));

	node->add_child_nocopy (*children);

	Glib::Mutex::Lock lm2 (controllables_lock);
	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
		children->add_child_nocopy ((*i)->get_state());
	}

	return *node;
}

int
GenericMidiControlProtocol::set_state (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	Controllable* c;

	{
		Glib::Mutex::Lock lm (pending_lock);
		pending_controllables.clear ();
	}

	Glib::Mutex::Lock lm2 (controllables_lock);

	controllables.clear ();

	nlist = node.children();

	if (nlist.empty()) {
		return 0;
	}

	nlist = nlist.front()->children ();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		XMLProperty* prop;

		if ((prop = (*niter)->property ("id")) != 0) {

			ID id = prop->value ();

			c = session->controllable_by_id (id);

			if (c) {
				MIDIControllable* mc = new MIDIControllable (*_port, *c);
				if (mc->set_state (**niter) == 0) {
					controllables.insert (mc);
				}
			}
		}
	}
	
	return 0;
}
