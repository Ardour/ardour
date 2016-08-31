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

#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/types_convert.h"
#include "pbd/xml++.h"
#include "pbd/compose.h"

#include "midi++/port.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/audioengine.h"
#include "ardour/controllable_descriptor.h"
#include "ardour/filesystem_paths.h"
#include "ardour/session.h"
#include "ardour/midi_ui.h"
#include "ardour/rc_configuration.h"
#include "ardour/midiport_manager.h"
#include "ardour/debug.h"

#include "generic_midi_control_protocol.h"
#include "midicontrollable.h"
#include "midifunction.h"
#include "midiaction.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

#include "pbd/i18n.h"

#define midi_ui_context() MidiControlUI::instance() /* a UICallback-derived object that specifies the event loop for signal handling */

GenericMidiControlProtocol::GenericMidiControlProtocol (Session& s)
	: ControlProtocol (s, _("Generic MIDI"))
	, connection_state (ConnectionState (0))
	, _motorised (false)
	, _threshold (10)
	, gui (0)
{
	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort> (s.midi_input_port ());
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort> (s.midi_output_port ());

	_input_bundle.reset (new ARDOUR::Bundle (_("Generic MIDI Control In"), true));
	_output_bundle.reset (new ARDOUR::Bundle (_("Generic MIDI Control Out"), false));

	_input_bundle->add_channel (
		boost::static_pointer_cast<MidiPort>(_input_port)->name(),
		ARDOUR::DataType::MIDI,
		session->engine().make_port_name_non_relative (boost::static_pointer_cast<MidiPort>(_input_port)->name())
		);

	_output_bundle->add_channel (
		boost::static_pointer_cast<MidiPort>(_output_port)->name(),
		ARDOUR::DataType::MIDI,
		session->engine().make_port_name_non_relative (boost::static_pointer_cast<MidiPort>(_output_port)->name())
		);

	session->BundleAddedOrRemoved ();

	do_feedback = false;
	_feedback_interval = 10000; // microseconds
	last_feedback_time = 0;

	_current_bank = 0;
	_bank_size = 0;

	/* these signals are emitted by the MidiControlUI's event loop thread
	 * and we may as well handle them right there in the same the same
	 * thread
	 */

	Controllable::StartLearning.connect_same_thread (*this, boost::bind (&GenericMidiControlProtocol::start_learning, this, _1));
	Controllable::StopLearning.connect_same_thread (*this, boost::bind (&GenericMidiControlProtocol::stop_learning, this, _1));
	Controllable::CreateBinding.connect_same_thread (*this, boost::bind (&GenericMidiControlProtocol::create_binding, this, _1, _2, _3));
	Controllable::DeleteBinding.connect_same_thread (*this, boost::bind (&GenericMidiControlProtocol::delete_binding, this, _1));

	/* this signal is emitted by the process() callback, and if
	 * send_feedback() is going to do anything, it should do it in the
	 * context of the process() callback itself.
	 */

	Session::SendFeedback.connect_same_thread (*this, boost::bind (&GenericMidiControlProtocol::send_feedback, this));
	//Session::SendFeedback.connect (*this, MISSING_INVALIDATOR, boost::bind (&GenericMidiControlProtocol::send_feedback, this), midi_ui_context());;

	/* this one is cross-thread */

	PresentationInfo::Change.connect (*this, MISSING_INVALIDATOR, boost::bind (&GenericMidiControlProtocol::reset_controllables, this), midi_ui_context());

	/* Catch port connections and disconnections (cross-thread) */
	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR,
	                                                                      boost::bind (&GenericMidiControlProtocol::connection_handler, this, _1, _2, _3, _4, _5),
	                                                                      midi_ui_context());

	reload_maps ();
}

GenericMidiControlProtocol::~GenericMidiControlProtocol ()
{
	drop_all ();
	tear_down_gui ();
}

list<boost::shared_ptr<ARDOUR::Bundle> >
GenericMidiControlProtocol::bundles ()
{
	list<boost::shared_ptr<ARDOUR::Bundle> > b;

	if (_input_bundle) {
		b.push_back (_input_bundle);
		b.push_back (_output_bundle);
	}

	return b;
}


static const char * const midimap_env_variable_name = "ARDOUR_MIDIMAPS_PATH";
static const char* const midi_map_dir_name = "midi_maps";
static const char* const midi_map_suffix = ".map";

Searchpath
system_midi_map_search_path ()
{
	bool midimap_path_defined = false;
	std::string spath_env (Glib::getenv (midimap_env_variable_name, midimap_path_defined));

	if (midimap_path_defined) {
		return spath_env;
	}

	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(midi_map_dir_name);
	return spath;
}

static std::string
user_midi_map_directory ()
{
	return Glib::build_filename (user_config_directory(), midi_map_dir_name);
}

static bool
midi_map_filter (const string &str, void* /*arg*/)
{
	return (str.length() > strlen(midi_map_suffix) &&
		str.find (midi_map_suffix) == (str.length() - strlen (midi_map_suffix)));
}

void
GenericMidiControlProtocol::reload_maps ()
{
	vector<string> midi_maps;
	Searchpath spath (system_midi_map_search_path());
	spath += user_midi_map_directory ();

	find_files_matching_filter (midi_maps, spath, midi_map_filter, 0, false, true);

	if (midi_maps.empty()) {
		cerr << "No MIDI maps found using " << spath.to_string() << endl;
		return;
	}

	for (vector<string>::iterator i = midi_maps.begin(); i != midi_maps.end(); ++i) {
		string fullpath = *i;

		XMLTree tree;

		if (!tree.read (fullpath.c_str())) {
			continue;
		}

		MapInfo mi;

		std::string str;
		if (!tree.root()->get_property ("name", str)) {
			continue;
		}

		mi.name = str;
		mi.path = fullpath;

		map_info.push_back (mi);
	}
}

void
GenericMidiControlProtocol::drop_all ()
{
	DEBUG_TRACE (DEBUG::GenericMidi, "Drop all bindings\n");
	Glib::Threads::Mutex::Lock lm (pending_lock);
	Glib::Threads::Mutex::Lock lm2 (controllables_lock);

	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
		delete *i;
	}
	controllables.clear ();

	for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ++i) {
		(*i)->connection.disconnect();
		if ((*i)->own_mc) {
			delete (*i)->mc;
		}
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
	DEBUG_TRACE (DEBUG::GenericMidi, "Drop bindings, leave learned\n");
	Glib::Threads::Mutex::Lock lm2 (controllables_lock);

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
	/* nothing to do here: the MIDI UI thread in libardour handles all our
	   I/O needs.
	*/
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

	_send_feedback ();

	last_feedback_time = now;
}

void
GenericMidiControlProtocol::_send_feedback ()
{
	/* This is executed in RT "process" context", so no blocking calls
	 */

	const int32_t bufsize = 16 * 1024; /* XXX too big */
	MIDI::byte buf[bufsize];
	int32_t bsize = bufsize;

	/* XXX: due to bugs in some ALSA / JACK MIDI bridges, we have to do separate
	   writes for each controllable here; if we send more than one MIDI message
	   in a single jack_midi_event_write then some bridges will only pass the
	   first on to ALSA.
	*/

	Glib::Threads::Mutex::Lock lm (controllables_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked ()) {
		return;
	}

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

	Glib::Threads::Mutex::Lock lm2 (controllables_lock);
	DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Learn binding: Controlable number: %1\n", c));

	/* drop any existing mappings for the same controllable for which
	 * learning has just started.
	 */

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

	/* check pending controllables (those for which a learn is underway) to
	 * see if it is for the same one for which learning has just started.
	 */

	{
		Glib::Threads::Mutex::Lock lm (pending_lock);

		for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ) {
			if (((*i)->mc)->get_controllable() == c) {
				(*i)->connection.disconnect();
				if ((*i)->own_mc) {
					delete (*i)->mc;
				}
				delete *i;
				i = pending_controllables.erase (i);
			} else {
				++i;
			}
		}
	}

	MIDIControllable* mc = 0;
	bool own_mc = false;

	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
		if ((*i)->get_controllable() && ((*i)->get_controllable()->id() == c->id())) {
			mc = *i;
			break;
		}
	}

	if (!mc) {
		mc = new MIDIControllable (this, *_input_port->parser(), *c, false);
		own_mc = true;
	}

	/* stuff the new controllable into pending */

	{
		Glib::Threads::Mutex::Lock lm (pending_lock);

		MIDIPendingControllable* element = new MIDIPendingControllable (mc, own_mc);
		c->LearningFinished.connect_same_thread (element->connection, boost::bind (&GenericMidiControlProtocol::learning_stopped, this, mc));

		pending_controllables.push_back (element);
	}
	mc->learn_about_external_control ();
	return true;
}

void
GenericMidiControlProtocol::learning_stopped (MIDIControllable* mc)
{
	Glib::Threads::Mutex::Lock lm (pending_lock);
	Glib::Threads::Mutex::Lock lm2 (controllables_lock);

	for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ) {
		if ( (*i)->mc == mc) {
			(*i)->connection.disconnect();
			delete *i;
			i = pending_controllables.erase(i);
		} else {
			++i;
		}
	}

	/* add the controllable for which learning stopped to our list of
	 * controllables
	 */

	controllables.push_back (mc);
}

void
GenericMidiControlProtocol::stop_learning (Controllable* c)
{
	Glib::Threads::Mutex::Lock lm (pending_lock);
	Glib::Threads::Mutex::Lock lm2 (controllables_lock);
	MIDIControllable* dptr = 0;

	/* learning timed out, and we've been told to consider this attempt to learn to be cancelled. find the
	   relevant MIDIControllable and remove it from the pending list.
	*/

	for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ++i) {
		if (((*i)->mc)->get_controllable() == c) {
			(*i)->mc->stop_learning ();
			dptr = (*i)->mc;
			(*i)->connection.disconnect();

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
		Glib::Threads::Mutex::Lock lm2 (controllables_lock);

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

// This next function seems unused
void
GenericMidiControlProtocol::create_binding (PBD::Controllable* control, int pos, int control_number)
{
	if (control != NULL) {
		Glib::Threads::Mutex::Lock lm2 (controllables_lock);

		MIDI::channel_t channel = (pos & 0xf);
		MIDI::byte value = control_number;

		// Create a MIDIControllable
		MIDIControllable* mc = new MIDIControllable (this, *_input_port->parser(), *control, false);

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
		DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Create binding: Channel: %1 Controller: %2 Value: %3 \n", channel, MIDI::controller, value));
		controllables.push_back (mc);
	}
}

void
GenericMidiControlProtocol::check_used_event (int pos, int control_number)
{
	Glib::Threads::Mutex::Lock lm2 (controllables_lock);

	MIDI::channel_t channel = (pos & 0xf);
	MIDI::byte value = control_number;

	DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("checking for used event: Channel: %1 Controller: %2 value: %3\n", (int) channel, (pos & 0xf0), (int) value));

	// Remove any old binding for this midi channel/type/value pair
	// Note:  can't use delete_binding() here because we don't know the specific controllable we want to remove, only the midi information
	for (MIDIControllables::iterator iter = controllables.begin(); iter != controllables.end();) {
		MIDIControllable* existingBinding = (*iter);
		if ( (existingBinding->get_control_type() & 0xf0 ) == (pos & 0xf0) && (existingBinding->get_control_channel() & 0xf ) == channel ) {
			if ( ((int) existingBinding->get_control_additional() == (int) value) || ((pos & 0xf0) == MIDI::pitchbend)) {
				DEBUG_TRACE (DEBUG::GenericMidi, "checking: found match, delete old binding.\n");
				delete existingBinding;
				iter = controllables.erase (iter);
			} else {
				++iter;
			}
		} else {
			++iter;
		}
	}

	for (MIDIFunctions::iterator iter = functions.begin(); iter != functions.end();) {
		MIDIFunction* existingBinding = (*iter);
		if ( (existingBinding->get_control_type() & 0xf0 ) == (pos & 0xf0) && (existingBinding->get_control_channel() & 0xf ) == channel ) {
			if ( ((int) existingBinding->get_control_additional() == (int) value) || ((pos & 0xf0) == MIDI::pitchbend)) {
				DEBUG_TRACE (DEBUG::GenericMidi, "checking: found match, delete old binding.\n");
				delete existingBinding;
				iter = functions.erase (iter);
			} else {
				++iter;
			}
		} else {
			++iter;
		}
	}

	for (MIDIActions::iterator iter = actions.begin(); iter != actions.end();) {
		MIDIAction* existingBinding = (*iter);
		if ( (existingBinding->get_control_type() & 0xf0 ) == (pos & 0xf0) && (existingBinding->get_control_channel() & 0xf ) == channel ) {
			if ( ((int) existingBinding->get_control_additional() == (int) value) || ((pos & 0xf0) == MIDI::pitchbend)) {
				DEBUG_TRACE (DEBUG::GenericMidi, "checking: found match, delete old binding.\n");
				delete existingBinding;
				iter = actions.erase (iter);
			} else {
				++iter;
			}
		} else {
			++iter;
		}
	}

}

XMLNode&
GenericMidiControlProtocol::get_state ()
{
	XMLNode& node (ControlProtocol::get_state());

	node.set_property (X_("feedback_interval"), _feedback_interval);
	node.set_property (X_("threshold"), _threshold);
	node.set_property (X_("motorized"), _motorised);

	if (!_current_binding.empty()) {
		node.set_property ("binding", _current_binding);
	}

	XMLNode* children = new XMLNode (X_("Controls"));

	node.add_child_nocopy (*children);

	Glib::Threads::Mutex::Lock lm2 (controllables_lock);
	for (MIDIControllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {

		/* we don't care about bindings that come from a bindings map, because
		   they will all be reset/recreated when we load the relevant bindings
		   file.
		*/

		if ((*i)->get_controllable() && (*i)->learned()) {
			children->add_child_nocopy ((*i)->get_state());
		}
	}

	return node;
}

int
GenericMidiControlProtocol::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	if (!node.get_property ("feedback_interval", _feedback_interval)) {
		_feedback_interval = 10000;
	}

	if (!node.get_property ("threshold", _threshold)) {
		_threshold = 10;
	}

	if (!node.get_property ("motorized", _motorised)) {
		_motorised = false;
	}

	boost::shared_ptr<Controllable> c;

	{
		Glib::Threads::Mutex::Lock lm (pending_lock);
		for (MIDIPendingControllables::iterator i = pending_controllables.begin(); i != pending_controllables.end(); ++i) {
			(*i)->connection.disconnect();
			if ((*i)->own_mc) {
				delete (*i)->mc;
			}
			delete *i;
		}
		pending_controllables.clear ();
	}

	std::string str;
	// midi map has to be loaded first so learned binding can go on top
	if (node.get_property ("binding", str)) {
		for (list<MapInfo>::iterator x = map_info.begin(); x != map_info.end(); ++x) {
			if (str == (*x).name) {
				load_bindings ((*x).path);
				break;
			}
		}
	}

	/* Load up specific bindings from the
	 * <Controls><MidiControllable>...</MidiControllable><Controls> section
	 */

	{
		Glib::Threads::Mutex::Lock lm2 (controllables_lock);
		nlist = node.children(); // "Controls"

		if (!nlist.empty()) {
			nlist = nlist.front()->children(); // "MIDIControllable" ...

			if (!nlist.empty()) {
				for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

					PBD::ID id;
					if ((*niter)->get_property ("id", id)) {

						DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Relearned binding for session: Control ID: %1\n", id.to_s()));
						Controllable* c = Controllable::by_id (id);

						if (c) {
							MIDIControllable* mc = new MIDIControllable (this, *_input_port->parser(), *c, false);

							if (mc->set_state (**niter, version) == 0) {
								controllables.push_back (mc);
							} else {
								warning << string_compose ("Generic MIDI control: Failed to set state for Control ID: %1\n", id.to_s());
								delete mc;
							}

						} else {
							warning << string_compose (
								_("Generic MIDI control: controllable %1 not found in session (ignored)"),
								id.to_s()) << endmsg;
						}
					}
				}
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
	DEBUG_TRACE (DEBUG::GenericMidi, "Load bindings: Reading midi map\n");
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
	}

	const XMLNodeList& children (root->children());
	XMLNodeConstIterator citer;
	XMLNodeConstIterator gciter;

	MIDIControllable* mc;

	drop_all ();

	DEBUG_TRACE (DEBUG::GenericMidi, "Loading bindings\n");
	for (citer = children.begin(); citer != children.end(); ++citer) {

		if ((*citer)->name() == "DeviceInfo") {

			if ((*citer)->get_property ("bank-size", _bank_size)) {
				_current_bank = 0;
			}

			if (!(*citer)->get_property ("motorized", _motorised)) {
				_motorised = false;
			}

			if (!(*citer)->get_property ("threshold", _threshold)) {
				_threshold = 10;
			}
		}

		if ((*citer)->name() == "Binding") {
			const XMLNode* child = *citer;

			if (child->property ("uri")) {
				/* controllable */

				Glib::Threads::Mutex::Lock lm2 (controllables_lock);
				if ((mc = create_binding (*child)) != 0) {
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
	MIDIControllable::Encoder encoder = MIDIControllable::No_enc;
	bool rpn_value = false;
	bool nrpn_value = false;
	bool rpn_change = false;
	bool nrpn_change = false;

	if ((prop = node.property (X_("ctl"))) != 0) {
		ev = MIDI::controller;
	} else if ((prop = node.property (X_("note"))) != 0) {
		ev = MIDI::on;
	} else if ((prop = node.property (X_("pgm"))) != 0) {
		ev = MIDI::program;
	} else if ((prop = node.property (X_("pb"))) != 0) {
		ev = MIDI::pitchbend;
	} else if ((prop = node.property (X_("enc-l"))) != 0) {
		encoder = MIDIControllable::Enc_L;
		ev = MIDI::controller;
	} else if ((prop = node.property (X_("enc-r"))) != 0) {
		encoder = MIDIControllable::Enc_R;
		ev = MIDI::controller;
	} else if ((prop = node.property (X_("enc-2"))) != 0) {
		encoder = MIDIControllable::Enc_2;
		ev = MIDI::controller;
	} else if ((prop = node.property (X_("enc-b"))) != 0) {
		encoder = MIDIControllable::Enc_B;
		ev = MIDI::controller;
	} else if ((prop = node.property (X_("rpn"))) != 0) {
		rpn_value = true;
	} else if ((prop = node.property (X_("nrpn"))) != 0) {
		nrpn_value = true;
	} else if ((prop = node.property (X_("rpn-delta"))) != 0) {
		rpn_change = true;
	} else if ((prop = node.property (X_("nrpn-delta"))) != 0) {
		nrpn_change = true;
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
		momentary = string_to<bool> (prop->value());
	} else {
		momentary = false;
	}

	prop = node.property (X_("uri"));
	uri = prop->value();

	MIDIControllable* mc = new MIDIControllable (this, *_input_port->parser(), momentary);

	if (mc->init (uri)) {
		delete mc;
		return 0;
	}

	if (rpn_value) {
		mc->bind_rpn_value (channel, detail);
	} else if (nrpn_value) {
		mc->bind_nrpn_value (channel, detail);
	} else if (rpn_change) {
		mc->bind_rpn_change (channel, detail);
	} else if (nrpn_change) {
		mc->bind_nrpn_change (channel, detail);
	} else {
		mc->set_encoder (encoder);
		mc->bind_midi (channel, ev, detail);
	}

	return mc;
}

void
GenericMidiControlProtocol::reset_controllables ()
{
	Glib::Threads::Mutex::Lock lm2 (controllables_lock);

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
			 * tracks). if we find this to be the case, we just leave
			 * the binding around, unbound, and it will do "late
			 * binding" (or "lazy binding") if/when any data arrives.
			 */

			existingBinding->lookup_controllable ();
		}

		iter = next;
	}
}

boost::shared_ptr<Controllable>
GenericMidiControlProtocol::lookup_controllable (const ControllableDescriptor& desc) const
{
	return session->controllable_by_descriptor (desc);
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

	MIDIFunction* mf = new MIDIFunction (*_input_port->parser());

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

	MIDIAction* ma = new MIDIAction (*_input_port->parser());

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

void
GenericMidiControlProtocol::set_motorised (bool m)
{
	_motorised = m;
}

void
GenericMidiControlProtocol::set_threshold (int t)
{
	_threshold = t;
}

bool
GenericMidiControlProtocol::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
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

	}

	ConnectionChange (); /* emit signal for our GUI */

	return true; /* connection status changed */
}

void
GenericMidiControlProtocol::connected ()
{
	cerr << "Now connected\n";
}

boost::shared_ptr<Port>
GenericMidiControlProtocol::output_port() const
{
	return _output_port;
}

boost::shared_ptr<Port>
GenericMidiControlProtocol::input_port() const
{
	return _input_port;
}

void
GenericMidiControlProtocol::maybe_start_touch (Controllable* controllable)
{
	AutomationControl *actl = dynamic_cast<AutomationControl*> (controllable);
	if (actl) {
		if (actl->automation_state() == Touch && !actl->touching()) {
			actl->start_touch (session->audible_frame ());
		}
	}
}

