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

#include <glibmm/miscutils.h>

#include "pbd/controllable_descriptor.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/pathscanner.h"
#include "pbd/xml++.h"

#include "midi++/port.h"
#include "midi++/manager.h"

#include "ardour/filesystem_paths.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/midi_ui.h"
#include "ardour/rc_configuration.h"

#include "generic_midi_control_protocol.h"
#include "midicontrollable.h"
#include "midifunction.h"
#include "midiaction.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

#include "i18n.h"

#define midi_ui_context() MidiControlUI::instance() /* a UICallback-derived object that specifies the event loop for signal handling */
#define ui_bind(x) boost::protect (boost::bind ((x)))

GenericMidiControlProtocol::GenericMidiControlProtocol (Session& s)
	: ControlProtocol (s, _("Generic MIDI"), midi_ui_context())
	, gui (0)
{

	_input_port = MIDI::Manager::instance()->midi_input_port ();
	_output_port = MIDI::Manager::instance()->midi_output_port ();

	do_feedback = false;
	_feedback_interval = 10000; // microseconds
	last_feedback_time = 0;

	_current_bank = 0;
	_bank_size = 0;

	/* XXX is it right to do all these in the same thread as whatever emits the signal? */

	Controllable::StartLearning.connect_same_thread (*this, boost::bind (&GenericMidiControlProtocol::start_learning, this, _1));
	Controllable::StopLearning.connect_same_thread (*this, boost::bind (&GenericMidiControlProtocol::stop_learning, this, _1));
	Controllable::CreateBinding.connect_same_thread (*this, boost::bind (&GenericMidiControlProtocol::create_binding, this, _1, _2, _3));
	Controllable::DeleteBinding.connect_same_thread (*this, boost::bind (&GenericMidiControlProtocol::delete_binding, this, _1));

	Session::SendFeedback.connect (*this, MISSING_INVALIDATOR, boost::bind (&GenericMidiControlProtocol::send_feedback, this), midi_ui_context());;
	Route::RemoteControlIDChange.connect (*this, MISSING_INVALIDATOR, boost::bind (&GenericMidiControlProtocol::reset_controllables, this), midi_ui_context());

	reload_maps ();
}

GenericMidiControlProtocol::~GenericMidiControlProtocol ()
{
	drop_all ();
	tear_down_gui ();
}

static const char * const midimap_env_variable_name = "ARDOUR_MIDIMAPS_PATH";
static const char* const midi_map_dir_name = "midi_maps";
static const char* const midi_map_suffix = ".map";

static sys::path
system_midi_map_search_path ()
{
	bool midimap_path_defined = false;
        sys::path spath_env (Glib::getenv (midimap_env_variable_name, midimap_path_defined));

	if (midimap_path_defined) {
		return spath_env;
	}

	SearchPath spath (system_data_search_path());
	spath.add_subdirectory_to_paths(midi_map_dir_name);

	// just return the first directory in the search path that exists
	SearchPath::const_iterator i = std::find_if(spath.begin(), spath.end(), sys::exists);

	if (i == spath.end()) return sys::path();

	return *i;
}

static sys::path
user_midi_map_directory ()
{
	sys::path p(user_config_directory());
	p /= midi_map_dir_name;

	return p;
}

static bool
midi_map_filter (const string &str, void */*arg*/)
{
	return (str.length() > strlen(midi_map_suffix) &&
		str.find (midi_map_suffix) == (str.length() - strlen (midi_map_suffix)));
}

void
GenericMidiControlProtocol::reload_maps ()
{
	vector<string *> *midi_maps;
	PathScanner scanner;
	SearchPath spath (system_midi_map_search_path());
	spath += user_midi_map_directory ();

	midi_maps = scanner (spath.to_string(), midi_map_filter, 0, false, true);

	if (!midi_maps) {
		cerr << "No MIDI maps found using " << spath.to_string() << endl;
		return;
	}

	for (vector<string*>::iterator i = midi_maps->begin(); i != midi_maps->end(); ++i) {
		string fullpath = *(*i);

		XMLTree tree;

		if (!tree.read (fullpath.c_str())) {
			continue;
		}

		MapInfo mi;

		XMLProperty* prop = tree.root()->property ("name");

		if (!prop) {
			continue;
		}

		mi.name = prop->value ();
		mi.path = fullpath;
		
		map_info.push_back (mi);
	}

	delete midi_maps;
}
	
void
GenericMidiControlProtocol::drop_all ()
{
	Glib::Mutex::Lock lm (pending_lock);
	Glib::Mutex::Lock lm2 (controllables_lock);

	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
		delete *i;
	}
	controllables.clear ();

	for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ++i) {
		delete *i;
	}
	pending_controllables.clear ();

	for (MIDIFunctions::iterator i = functions.begin(); i != functions.end(); ++i) {
		delete *i;
	}
	functions.clear ();

	for (MIDIActions::iterator i = actions.begin(); i != actions.end(); ++i) {
		delete *i;
	}
	actions.clear ();
}

void
GenericMidiControlProtocol::drop_bindings ()
{
	Glib::Mutex::Lock lm2 (controllables_lock);

	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ) {
		if (!(*i)->learned()) {
			delete *i;
			i = controllables.erase (i);
		} else {
			++i;
		}
	}

	for (MIDIFunctions::iterator i = functions.begin(); i != functions.end(); ++i) {
		delete *i;
	}
	functions.clear ();

	_current_binding = "";
	_bank_size = 0;
	_current_bank = 0;
}

int
GenericMidiControlProtocol::set_active (bool /*yn*/)
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

	/* XXX: due to bugs in some ALSA / JACK MIDI bridges, we have to do separate
	   writes for each controllable here; if we send more than one MIDI message
	   in a single jack_midi_event_write then some bridges will only pass the
	   first on to ALSA.
	*/
	for (MIDIControllables::iterator r = controllables.begin(); r != controllables.end(); ++r) {
		MIDI::byte* end = (*r)->write_feedback (buf, bsize);
		if (end != buf) {
			_output_port->write (buf, (int32_t) (end - buf), 0);
		}
	}
}

bool
GenericMidiControlProtocol::start_learning (Controllable* c)
{
	if (c == 0) {
		return false;
	}

	Glib::Mutex::Lock lm2 (controllables_lock);

	MIDIControllables::iterator tmp;
	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ) {
		tmp = i;
		++tmp;
		if ((*i)->get_controllable() == c) {
			delete (*i);
			controllables.erase (i);
		}
		i = tmp;
	}

	{
		Glib::Mutex::Lock lm (pending_lock);
		
		MIDIPendingControllables::iterator ptmp;
		for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ) {
			ptmp = i;
			++ptmp;
			if (((*i)->first)->get_controllable() == c) {
				(*i)->second.disconnect();
				delete (*i)->first;
				delete *i;
				pending_controllables.erase (i);
			}
			i = ptmp;
		}
	}

	MIDIControllable* mc = 0;

	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
		if ((*i)->get_controllable() && ((*i)->get_controllable()->id() == c->id())) {
			mc = *i;
			break;
		}
	}

	if (!mc) {
		mc = new MIDIControllable (*_input_port, *c, false);
	}
	
	{
		Glib::Mutex::Lock lm (pending_lock);

		MIDIPendingControllable* element = new MIDIPendingControllable;
		element->first = mc;
		c->LearningFinished.connect_same_thread (element->second, boost::bind (&GenericMidiControlProtocol::learning_stopped, this, mc));

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

		if ( (*i)->first == mc) {
			(*i)->second.disconnect();
			delete *i;
			pending_controllables.erase(i);
		}

		i = tmp;
	}

	controllables.push_back (mc);
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
		if (((*i)->first)->get_controllable() == c) {
			(*i)->first->stop_learning ();
			dptr = (*i)->first;
			(*i)->second.disconnect();

			delete *i;
			pending_controllables.erase (i);
			break;
		}
	}
	
	delete dptr;
}

void
GenericMidiControlProtocol::delete_binding (PBD::Controllable* control)
{
	if (control != 0) {
		Glib::Mutex::Lock lm2 (controllables_lock);
		
		for (MIDIControllables::iterator iter = controllables.begin(); iter != controllables.end();) {
			MIDIControllable* existingBinding = (*iter);
			
			if (control == (existingBinding->get_controllable())) {
				delete existingBinding;
				iter = controllables.erase (iter);
			} else {
				++iter;
			}
			
		}
	}
}

void
GenericMidiControlProtocol::create_binding (PBD::Controllable* control, int pos, int control_number)
{
	if (control != NULL) {
		Glib::Mutex::Lock lm2 (controllables_lock);
		
		MIDI::channel_t channel = (pos & 0xf);
		MIDI::byte value = control_number;
		
		// Create a MIDIControllable
		MIDIControllable* mc = new MIDIControllable (*_input_port, *control, false);

		// Remove any old binding for this midi channel/type/value pair
		// Note:  can't use delete_binding() here because we don't know the specific controllable we want to remove, only the midi information
		for (MIDIControllables::iterator iter = controllables.begin(); iter != controllables.end();) {
			MIDIControllable* existingBinding = (*iter);
			
			if ((existingBinding->get_control_channel() & 0xf ) == channel &&
			    existingBinding->get_control_additional() == value &&
			    (existingBinding->get_control_type() & 0xf0 ) == MIDI::controller) {
				
				delete existingBinding;
				iter = controllables.erase (iter);
			} else {
				++iter;
			}
			
		}
		
		// Update the MIDI Controllable based on the the pos param
		// Here is where a table lookup for user mappings could go; for now we'll just wing it...
		mc->bind_midi(channel, MIDI::controller, value);

		controllables.push_back (mc);
	}
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

	if (!_current_binding.empty()) {
		node->add_property ("binding", _current_binding);
	}

	XMLNode* children = new XMLNode (X_("Controls"));

	node->add_child_nocopy (*children);

	Glib::Mutex::Lock lm2 (controllables_lock);
	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {

		/* we don't care about bindings that come from a bindings map, because
		   they will all be reset/recreated when we load the relevant bindings
		   file.
		*/

		if ((*i)->learned()) {
			children->add_child_nocopy ((*i)->get_state());
		}
	}

	return *node;
}

int
GenericMidiControlProtocol::set_state (const XMLNode& node, int version)
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

	boost::shared_ptr<Controllable> c;
	
	{
		Glib::Mutex::Lock lm (pending_lock);
		for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ++i) {
			delete *i;
		}
		pending_controllables.clear ();
	}

	{
		Glib::Mutex::Lock lm2 (controllables_lock);
		controllables.clear ();
		nlist = node.children(); // "Controls"
		
		if (nlist.empty()) {
			return 0;
		}
		
		nlist = nlist.front()->children ();
		
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			
			if ((prop = (*niter)->property ("id")) != 0) {

                                cerr << "Looking for MIDI Controllable with ID " << prop->value() << endl;
				
				ID id = prop->value ();
				Controllable* c = Controllable::by_id (id);

                                cerr << "\tresult = " << c << endl;
				
				if (c) {
					MIDIControllable* mc = new MIDIControllable (*_input_port, *c, false);
                                        
					if (mc->set_state (**niter, version) == 0) {
						controllables.push_back (mc);
					}
					
				} else {
					warning << string_compose (
						_("Generic MIDI control: controllable %1 not found in session (ignored)"),
						id) << endmsg;
				}
			}
		}

	}

	if ((prop = node.property ("binding")) != 0) {
		for (list<MapInfo>::iterator x = map_info.begin(); x != map_info.end(); ++x) {
			if (prop->value() == (*x).name) {
				load_bindings ((*x).path);
				break;
			}
		}
	}

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

int
GenericMidiControlProtocol::load_bindings (const string& xmlpath)
{
	XMLTree state_tree;

	if (!state_tree.read (xmlpath.c_str())) {
		error << string_compose(_("Could not understand MIDI bindings file %1"), xmlpath) << endmsg;
		return -1;
	}

	XMLNode* root = state_tree.root();

	if (root->name() != X_("ArdourMIDIBindings")) {
		error << string_compose (_("MIDI Bindings file %1 is not really a MIDI bindings file"), xmlpath) << endmsg;
		return -1;
	}

	const XMLProperty* prop;

	if ((prop = root->property ("version")) == 0) {
		return -1;
	} else {
		int major;
		int minor;
		int micro;

		sscanf (prop->value().c_str(), "%d.%d.%d", &major, &minor, &micro);
		Stateful::loading_state_version = (major * 1000) + minor;
	}
	
	const XMLNodeList& children (root->children());
	XMLNodeConstIterator citer;
	XMLNodeConstIterator gciter;

	MIDIControllable* mc;

	drop_all ();

	for (citer = children.begin(); citer != children.end(); ++citer) {
		
		if ((*citer)->name() == "DeviceInfo") {
			const XMLProperty* prop;

			if ((prop = (*citer)->property ("bank-size")) != 0) {
				_bank_size = atoi (prop->value());
				_current_bank = 0;
			}
		}

		if ((*citer)->name() == "Binding") {
			const XMLNode* child = *citer;

			if (child->property ("uri")) {
				/* controllable */
				
				if ((mc = create_binding (*child)) != 0) {
					Glib::Mutex::Lock lm2 (controllables_lock);
					controllables.push_back (mc);
				}

			} else if (child->property ("function")) {

				/* function */
				MIDIFunction* mf;

				if ((mf = create_function (*child)) != 0) {
					functions.push_back (mf);
				}

			} else if (child->property ("action")) {
                                MIDIAction* ma;

				if ((ma = create_action (*child)) != 0) {
					actions.push_back (ma);
				}
                        }
		}
	}
	
	if ((prop = root->property ("name")) != 0) {
		_current_binding = prop->value ();
	}

	reset_controllables ();

	return 0;
}

MIDIControllable*
GenericMidiControlProtocol::create_binding (const XMLNode& node)
{
	const XMLProperty* prop;
	MIDI::byte detail;
	MIDI::channel_t channel;
	string uri;
	MIDI::eventType ev;
	int intval;
	bool momentary;

	if ((prop = node.property (X_("ctl"))) != 0) {
		ev = MIDI::controller;
	} else if ((prop = node.property (X_("note"))) != 0) {
		ev = MIDI::on;
	} else if ((prop = node.property (X_("pgm"))) != 0) {
		ev = MIDI::program;
	} else if ((prop = node.property (X_("pb"))) != 0) {
		ev = MIDI::pitchbend;
	} else {
		return 0;
	}
	
	if (sscanf (prop->value().c_str(), "%d", &intval) != 1) {
		return 0;
	}
	
	detail = (MIDI::byte) intval;

	if ((prop = node.property (X_("channel"))) == 0) {
		return 0;
	}
	
	if (sscanf (prop->value().c_str(), "%d", &intval) != 1) {
		return 0;
	}
	channel = (MIDI::channel_t) intval;
	/* adjust channel to zero-based counting */
	if (channel > 0) {
		channel -= 1;
	}

	if ((prop = node.property (X_("momentary"))) != 0) {
		momentary = string_is_affirmative (prop->value());
	} else {
		momentary = false;
	}
	
	prop = node.property (X_("uri"));
	uri = prop->value();

	MIDIControllable* mc = new MIDIControllable (*_input_port, momentary);

	if (mc->init (uri)) {
		delete mc;
		return 0;
	}

	mc->bind_midi (channel, ev, detail);

	return mc;
}

void
GenericMidiControlProtocol::reset_controllables ()
{
	Glib::Mutex::Lock lm2 (controllables_lock);

	for (MIDIControllables::iterator iter = controllables.begin(); iter != controllables.end(); ) {
		MIDIControllable* existingBinding = (*iter);
		MIDIControllables::iterator next = iter;
		++next;

		if (!existingBinding->learned()) {
			ControllableDescriptor& desc (existingBinding->descriptor());

			if (desc.banked()) {
				desc.set_bank_offset (_current_bank * _bank_size);
			}

			/* its entirely possible that the session doesn't have
			 * the specified controllable (e.g. it has too few
			 * tracks). if we find this to be the case, drop any
			 * bindings that would be left without controllables.
			 */

			boost::shared_ptr<Controllable> c = session->controllable_by_descriptor (desc);
			if (c) {
				existingBinding->set_controllable (c.get());
			} else {
				controllables.erase (iter);
			}
		}

		iter = next;
	}
}

MIDIFunction*
GenericMidiControlProtocol::create_function (const XMLNode& node)
{
	const XMLProperty* prop;
	int intval;
	MIDI::byte detail = 0;
	MIDI::channel_t channel = 0;
	string uri;
	MIDI::eventType ev;
	MIDI::byte* data = 0;
	uint32_t data_size = 0;
	string argument;

	if ((prop = node.property (X_("ctl"))) != 0) {
		ev = MIDI::controller;
	} else if ((prop = node.property (X_("note"))) != 0) {
		ev = MIDI::on;
	} else if ((prop = node.property (X_("pgm"))) != 0) {
		ev = MIDI::program;
	} else if ((prop = node.property (X_("sysex"))) != 0 || (prop = node.property (X_("msg"))) != 0) {

                if (prop->name() == X_("sysex")) {
                        ev = MIDI::sysex;
                } else {
                        ev = MIDI::any;
                }

		int val;
		uint32_t cnt;

		{
			cnt = 0;
			stringstream ss (prop->value());
			ss << hex;
			
			while (ss >> val) {
				cnt++;
			}
		}

		if (cnt == 0) {
			return 0;
		}

		data = new MIDI::byte[cnt];
		data_size = cnt;
		
		{
			stringstream ss (prop->value());
			ss << hex;
			cnt = 0;
			
			while (ss >> val) {
				data[cnt++] = (MIDI::byte) val;
			}
		}

	} else {
		warning << "Binding ignored - unknown type" << endmsg;
		return 0;
	}

	if (data_size == 0) {
		if (sscanf (prop->value().c_str(), "%d", &intval) != 1) {
			return 0;
		}
		
		detail = (MIDI::byte) intval;

		if ((prop = node.property (X_("channel"))) == 0) {
			return 0;
		}
	
		if (sscanf (prop->value().c_str(), "%d", &intval) != 1) {
			return 0;
		}
		channel = (MIDI::channel_t) intval;
		/* adjust channel to zero-based counting */
		if (channel > 0) {
			channel -= 1;
		}
	}

	if ((prop = node.property (X_("arg"))) != 0 || (prop = node.property (X_("argument"))) != 0 || (prop = node.property (X_("arguments"))) != 0) {
		argument = prop->value ();
	}

	prop = node.property (X_("function"));
	
	MIDIFunction* mf = new MIDIFunction (*_input_port);
	
	if (mf->setup (*this, prop->value(), argument, data, data_size)) {
		delete mf;
		return 0;
	}

	mf->bind_midi (channel, ev, detail);

	return mf;
}

MIDIAction*
GenericMidiControlProtocol::create_action (const XMLNode& node)
{
	const XMLProperty* prop;
	int intval;
	MIDI::byte detail = 0;
	MIDI::channel_t channel = 0;
	string uri;
	MIDI::eventType ev;
	MIDI::byte* data = 0;
	uint32_t data_size = 0;

	if ((prop = node.property (X_("ctl"))) != 0) {
		ev = MIDI::controller;
	} else if ((prop = node.property (X_("note"))) != 0) {
		ev = MIDI::on;
	} else if ((prop = node.property (X_("pgm"))) != 0) {
		ev = MIDI::program;
	} else if ((prop = node.property (X_("sysex"))) != 0 || (prop = node.property (X_("msg"))) != 0) {

                if (prop->name() == X_("sysex")) {
                        ev = MIDI::sysex;
                } else {
                        ev = MIDI::any;
                }

		int val;
		uint32_t cnt;

		{
			cnt = 0;
			stringstream ss (prop->value());
			ss << hex;
			
			while (ss >> val) {
				cnt++;
			}
		}

		if (cnt == 0) {
			return 0;
		}

		data = new MIDI::byte[cnt];
		data_size = cnt;
		
		{
			stringstream ss (prop->value());
			ss << hex;
			cnt = 0;
			
			while (ss >> val) {
				data[cnt++] = (MIDI::byte) val;
			}
		}

	} else {
		warning << "Binding ignored - unknown type" << endmsg;
		return 0;
	}

	if (data_size == 0) {
		if (sscanf (prop->value().c_str(), "%d", &intval) != 1) {
			return 0;
		}
		
		detail = (MIDI::byte) intval;

		if ((prop = node.property (X_("channel"))) == 0) {
			return 0;
		}
	
		if (sscanf (prop->value().c_str(), "%d", &intval) != 1) {
			return 0;
		}
		channel = (MIDI::channel_t) intval;
		/* adjust channel to zero-based counting */
		if (channel > 0) {
			channel -= 1;
		}
	}

	prop = node.property (X_("action"));
	
	MIDIAction* ma = new MIDIAction (*_input_port);
        
	if (ma->init (*this, prop->value(), data, data_size)) {
		delete ma;
		return 0;
	}

	ma->bind_midi (channel, ev, detail);

	return ma;
}

void
GenericMidiControlProtocol::set_current_bank (uint32_t b)
{
	_current_bank = b;
	reset_controllables ();
}

void
GenericMidiControlProtocol::next_bank ()
{
	_current_bank++;
	reset_controllables ();
}

void
GenericMidiControlProtocol::prev_bank()
{
	if (_current_bank) {
		_current_bank--;
		reset_controllables ();
	}
}
