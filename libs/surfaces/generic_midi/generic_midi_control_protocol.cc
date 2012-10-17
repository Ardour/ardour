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

#include <algorithm>

#include <pbd/error.h>
#include <pbd/failed_constructor.h>

#include <midi++/port.h>
#include <midi++/manager.h>

#include <ardour/route.h>
#include <ardour/session.h>

#include "generic_midi_control_protocol.h"
#include "midicontrollable.h"

using namespace ARDOUR;
using namespace PBD;

#include "i18n.h"

GenericMidiControlProtocol::GenericMidiControlProtocol (Session& s)
	: ControlProtocol  (s, _("Generic MIDI"))
{
	MIDI::Manager* mm = MIDI::Manager::instance();

        _port = mm->port (Config->get_midi_port_name());

	if (_port == 0) {
                error << string_compose (_("no MIDI port named \"%1\" exists - generic MIDI control disabled"), Config->get_midi_port_name())
                      << endmsg;
		throw failed_constructor();
	}

	do_feedback = false;
	_feedback_interval = 10000; // microseconds
	last_feedback_time = 0;

	auto_binding = false;

	Controllable::StartLearning.connect (mem_fun (*this, &GenericMidiControlProtocol::start_learning));
	Controllable::StopLearning.connect (mem_fun (*this, &GenericMidiControlProtocol::stop_learning));
	Session::SendFeedback.connect (mem_fun (*this, &GenericMidiControlProtocol::send_feedback));
	
	Controllable::CreateBinding.connect (mem_fun (*this, &GenericMidiControlProtocol::create_binding));
	Controllable::DeleteBinding.connect (mem_fun (*this, &GenericMidiControlProtocol::delete_binding));

	Session::AutoBindingOn.connect (mem_fun (*this, &GenericMidiControlProtocol::auto_binding_on));
	Session::AutoBindingOff.connect (mem_fun (*this, &GenericMidiControlProtocol::auto_binding_off));
}

GenericMidiControlProtocol::~GenericMidiControlProtocol ()
{
	Glib::Mutex::Lock lm2 (controllables_lock);
	
	for (MIDIControllables::iterator iter = controllables.begin(); iter != controllables.end(); ++iter) {
		MIDIControllable* existingBinding = (*iter);
		delete existingBinding;
	}
}

int
GenericMidiControlProtocol::set_active (bool yn)
{
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
	if (!do_feedback) {
		return;
	}

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
	const int32_t bufsize = 16 * 1024; /* XXX too big */
	MIDI::byte buf[bufsize];
	int32_t bsize = bufsize;
	MIDI::byte* end = buf;
	
	for (MIDIControllables::iterator r = controllables.begin(); r != controllables.end(); ++r) {
		end = (*r)->write_feedback (end, bsize);
	}
	
	if (end == buf) {
		return;
	} 

	_port->write (buf, (int32_t) (end - buf));
}

bool
GenericMidiControlProtocol::start_learning (Controllable* c)
{
	if (c == 0) {
		return false;
	}

	MIDIControllables::iterator tmp;
	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ) {
		tmp = i;
		++tmp;
		if (&(*i)->get_controllable() == c) {
			delete (*i);
			controllables.erase (i);
		}
		i = tmp;
	}

	MIDIPendingControllables::iterator ptmp;
	for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ) {
		ptmp = i;
		++ptmp;
		if (&((*i).first)->get_controllable() == c) {
			(*i).second.disconnect();
			delete (*i).first;
			pending_controllables.erase (i);
		}
		i = ptmp;
	}


	MIDIControllable* mc = 0;

	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
		if ((*i)->get_controllable().id() == c->id()) {
			mc = *i;
			break;
		}
	}

	if (!mc) {
		mc = new MIDIControllable (*_port, *c);
	}
	
	{
		Glib::Mutex::Lock lm (pending_lock);

		std::pair<MIDIControllable *, sigc::connection> element;
		element.first = mc;
		element.second = c->LearningFinished.connect (bind (mem_fun (*this, &GenericMidiControlProtocol::learning_stopped), mc));

		pending_controllables.push_back (element);
	}

	mc->learn_about_external_control ();
	return true;
}

void
GenericMidiControlProtocol::learning_stopped (MIDIControllable* mc)
{
	Glib::Mutex::Lock lm (pending_lock);
	Glib::Mutex::Lock lm2 (controllables_lock);
	
	MIDIPendingControllables::iterator tmp;

	for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ) {
		tmp = i;
		++tmp;

		if ( (*i).first == mc) {
			(*i).second.disconnect();
			pending_controllables.erase(i);
		}

		i = tmp;
	}

	controllables.insert (mc);
}

void
GenericMidiControlProtocol::stop_learning (Controllable* c)
{
	Glib::Mutex::Lock lm (pending_lock);
	Glib::Mutex::Lock lm2 (controllables_lock);
	MIDIControllable* dptr = 0;

	/* learning timed out, and we've been told to consider this attempt to learn to be cancelled. find the
	   relevant MIDIControllable and remove it from the pending list.
	*/

	for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ++i) {
		if (&((*i).first)->get_controllable() == c) {
			(*i).first->stop_learning ();
			dptr = (*i).first;
			(*i).second.disconnect();

			pending_controllables.erase (i);
			break;
		}
	}
	
	delete dptr;
}

void
GenericMidiControlProtocol::delete_binding ( PBD::Controllable* control )
{
	if( control != 0 ) {
		Glib::Mutex::Lock lm2 (controllables_lock);
		
		for( MIDIControllables::iterator iter = controllables.begin(); iter != controllables.end(); ++iter) {
			MIDIControllable* existingBinding = (*iter);
			
			if( control == &(existingBinding->get_controllable()) ) {
				delete existingBinding;
				controllables.erase (iter);
			}
			
		} // end for midi controllables
	} // end null check
}
void
GenericMidiControlProtocol::create_binding (PBD::Controllable* control, int pos, int control_number)
{
	if( control != NULL ) {
		Glib::Mutex::Lock lm2 (controllables_lock);
		
		MIDI::channel_t channel = (pos & 0xf);
		MIDI::byte value = control_number;
		
		// Create a MIDIControllable::
		MIDIControllable* mc = new MIDIControllable (*_port, *control);
		
		// Remove any old binding for this midi channel/type/value pair
		// Note:  can't use delete_binding() here because we don't know the specific controllable we want to remove, only the midi information
		for( MIDIControllables::iterator iter = controllables.begin(); iter != controllables.end(); ++iter) {
			MIDIControllable* existingBinding = (*iter);
			
			if( (existingBinding->get_control_channel() & 0xf ) == channel &&
			    existingBinding->get_control_additional() == value &&
			    (existingBinding->get_control_type() & 0xf0 ) == MIDI::controller ) {
				
				delete existingBinding;
				controllables.erase (iter);
			}
			
		} // end for midi controllables
		
		
		// Update the MIDI Controllable based on the the pos param
		// Here is where a table lookup for user mappings could go; for now we'll just wing it...
		mc->bind_midi( channel, MIDI::controller, value );
		
		controllables.insert (mc);
	} // end null test
}

void
GenericMidiControlProtocol::auto_binding_on()
{
	auto_binding = true;
}

void
GenericMidiControlProtocol::auto_binding_off()
{
	auto_binding = false;
}

XMLNode&
GenericMidiControlProtocol::get_state () 
{
	XMLNode* node = new XMLNode ("Protocol"); 
	char buf[32];

	node->add_property (X_("name"), _name);
	node->add_property (X_("feedback"), do_feedback ? "1" : "0");
	snprintf (buf, sizeof (buf), "%" PRIu64, _feedback_interval);
	node->add_property (X_("feedback_interval"), buf);

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
	const XMLProperty* prop;

	if ((prop = node.property ("feedback")) != 0) {
		do_feedback = (bool) atoi (prop->value().c_str());
	} else {
		do_feedback = false;
	}

	if ((prop = node.property ("feedback_interval")) != 0) {
		if (sscanf (prop->value().c_str(), "%" PRIu64, &_feedback_interval) != 1) {
			_feedback_interval = 10000;
		}
	} else {
		_feedback_interval = 10000;
	}

	if ( !auto_binding ) {
		
		Controllable* c;
		
		{
			Glib::Mutex::Lock lm (pending_lock);
			pending_controllables.clear ();
		}
		
		Glib::Mutex::Lock lm2 (controllables_lock);
		nlist = node.children(); // "controls"
		
		controllables.clear ();

		if (nlist.empty()) {
			return 0;
		}
		
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			
			if ((prop = (*niter)->property ("id")) != 0) {

				ID id = prop->value ();
				c = session->controllable_by_id (id);
				
				if (c) {
					MIDIControllable* mc = new MIDIControllable (*_port, *c);
					if (mc->set_state (**niter) == 0) {
						controllables.insert (mc);
					}
					
				} else {
					warning << string_compose (_("Generic MIDI control: controllable %1 not found in session (ignored)"),
								   id)
						<< endmsg;
				}
			}
		}
	} // end autobinding check
	return 0;
}

int
GenericMidiControlProtocol::set_feedback (bool yn)
{
	do_feedback = yn;
	last_feedback_time = 0;
	return 0;
}

bool
GenericMidiControlProtocol::get_feedback () const
{
	return do_feedback;
}
