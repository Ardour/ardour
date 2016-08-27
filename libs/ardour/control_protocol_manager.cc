/*
    Copyright (C) 2000-2007 Paul Davis

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

#include <glibmm/module.h>

#include <glibmm/fileutils.h>

#include "pbd/compose.h"
#include "pbd/event_loop.h"
#include "pbd/file_utils.h"
#include "pbd/error.h"

#include "control_protocol/control_protocol.h"

#include "ardour/debug.h"
#include "ardour/control_protocol_manager.h"

#include "ardour/search_paths.h"


using namespace ARDOUR;
using namespace std;
using namespace PBD;

#include "pbd/i18n.h"

ControlProtocolManager* ControlProtocolManager::_instance = 0;
const string ControlProtocolManager::state_node_name = X_("ControlProtocols");


ControlProtocolInfo::~ControlProtocolInfo ()
{
	if (protocol && descriptor) {
		descriptor->destroy (descriptor, protocol);
		protocol = 0;
	}

	delete state; state = 0;

	if (descriptor) {
		delete (Glib::Module*) descriptor->module;
		descriptor = 0;
	}
}

ControlProtocolManager::ControlProtocolManager ()
{
}

ControlProtocolManager::~ControlProtocolManager()
{
	Glib::Threads::Mutex::Lock lm (protocols_lock);

	for (list<ControlProtocol*>::iterator i = control_protocols.begin(); i != control_protocols.end(); ++i) {
		delete (*i);
	}

	control_protocols.clear ();


	for (list<ControlProtocolInfo*>::iterator p = control_protocol_info.begin(); p != control_protocol_info.end(); ++p) {
		delete (*p);
	}

	control_protocol_info.clear();
}

void
ControlProtocolManager::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {
		Glib::Threads::Mutex::Lock lm (protocols_lock);

		for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
			if ((*i)->requested || (*i)->mandatory) {
				(void) activate (**i);
			}
		}
	}
}

int
ControlProtocolManager::activate (ControlProtocolInfo& cpi)
{
	ControlProtocol* cp;

	cpi.requested = true;

	if ((cp = instantiate (cpi)) == 0) {
		return -1;
	}

	/* we split the set_state() and set_active() operations so that
	   protocols that need state to configure themselves (e.g. "What device
	   is connected, or supposed to be connected?") can get it before
	   actually starting any interaction.
	*/

	if (cpi.state) {
		/* force this by tweaking the internals of the state
		 * XMLNode. Ugh.
		 */
		cp->set_state (*cpi.state, Stateful::loading_state_version);
	} else {
		/* guarantee a call to
		   set_state() whether we have
		   existing state or not
		*/
		cp->set_state (XMLNode(""), Stateful::loading_state_version);
	}

	if (cp->set_active (true)) {
		error << string_compose (_("Control protocol support for %1 failed to activate"), cpi.name) << endmsg;
		teardown (cpi, false);
	}

	return 0;
}

int
ControlProtocolManager::deactivate (ControlProtocolInfo& cpi)
{
	cpi.requested = false;
	return teardown (cpi, true);
}

void
ControlProtocolManager::session_going_away()
{
	SessionHandlePtr::session_going_away ();
	/* Session::destroy() will explicitly call drop_protocols() so we don't
	 * have to worry about that here.
	 */
}

void
ControlProtocolManager::drop_protocols ()
{
	/* called explicitly by Session::destroy() so that we can clean up
	 * before the process cycle stops and ports vanish.
	 */

	Glib::Threads::Mutex::Lock lm (protocols_lock);

	for (list<ControlProtocol*>::iterator p = control_protocols.begin(); p != control_protocols.end(); ++p) {
		delete *p;
	}

	control_protocols.clear ();

	for (list<ControlProtocolInfo*>::iterator p = control_protocol_info.begin(); p != control_protocol_info.end(); ++p) {
		// mark existing protocols as requested
		// otherwise the ControlProtocol instances are not recreated in set_session
		if ((*p)->protocol) {
			(*p)->requested = true;
			(*p)->protocol = 0;
		}
	}
}

ControlProtocol*
ControlProtocolManager::instantiate (ControlProtocolInfo& cpi)
{
	/* CALLER MUST HOLD LOCK */

	if (_session == 0) {
		return 0;
	}

	cpi.descriptor = get_descriptor (cpi.path);

	DEBUG_TRACE (DEBUG::ControlProtocols, string_compose ("instantiating %1\n", cpi.name));

	if (cpi.descriptor == 0) {
		error << string_compose (_("control protocol name \"%1\" has no descriptor"), cpi.name) << endmsg;
		return 0;
	}

	DEBUG_TRACE (DEBUG::ControlProtocols, string_compose ("initializing %1\n", cpi.name));

	if ((cpi.protocol = cpi.descriptor->initialize (cpi.descriptor, _session)) == 0) {
		error << string_compose (_("control protocol name \"%1\" could not be initialized"), cpi.name) << endmsg;
		return 0;
	}

	control_protocols.push_back (cpi.protocol);

	ProtocolStatusChange (&cpi);

	return cpi.protocol;
}

int
ControlProtocolManager::teardown (ControlProtocolInfo& cpi, bool lock_required)
{
	if (!cpi.protocol) {

		/* we could still have a descriptor even if the protocol was
		   never instantiated. Close the associated module (shared
		   object/DLL) and make sure we forget about it.
		*/

		if (cpi.descriptor) {
			cerr << "Closing descriptor for CPI anyway\n";
			delete (Glib::Module*) cpi.descriptor->module;
			cpi.descriptor = 0;
		}

		return 0;
	}

	if (!cpi.descriptor) {
		return 0;
	}

	if (cpi.mandatory) {
		return 0;
	}

	/* save current state */

	delete cpi.state;
	cpi.state = new XMLNode (cpi.protocol->get_state());
	cpi.state->set_property (X_("active"), "no");

	cpi.descriptor->destroy (cpi.descriptor, cpi.protocol);

	if (lock_required) {
		Glib::Threads::Mutex::Lock lm (protocols_lock);
		list<ControlProtocol*>::iterator p = find (control_protocols.begin(), control_protocols.end(), cpi.protocol);
		if (p != control_protocols.end()) {
			control_protocols.erase (p);
		} else {
			cerr << "Programming error: ControlProtocolManager::teardown() called for " << cpi.name << ", but it was not found in control_protocols" << endl;
		}
	} else {
		list<ControlProtocol*>::iterator p = find (control_protocols.begin(), control_protocols.end(), cpi.protocol);
		if (p != control_protocols.end()) {
			control_protocols.erase (p);
		} else {
			cerr << "Programming error: ControlProtocolManager::teardown() called for " << cpi.name << ", but it was not found in control_protocols" << endl;
		}
	}

	cpi.protocol = 0;

	delete cpi.state;
	cpi.state = 0;
	delete (Glib::Module*) cpi.descriptor->module;
	/* cpi->descriptor is now inaccessible since dlclose() or equivalent
	 * has been performed, and the descriptor is (or could be) a static
	 * object made accessible by dlopen().
	 */
	cpi.descriptor = 0;

	ProtocolStatusChange (&cpi);

	return 0;
}

void
ControlProtocolManager::load_mandatory_protocols ()
{
	if (_session == 0) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (protocols_lock);

	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
		if ((*i)->mandatory && ((*i)->protocol == 0)) {
			DEBUG_TRACE (DEBUG::ControlProtocols,
				     string_compose (_("Instantiating mandatory control protocol %1"), (*i)->name));
			instantiate (**i);
		}
	}
}

void
ControlProtocolManager::discover_control_protocols ()
{
	vector<std::string> cp_modules;

#ifdef COMPILER_MSVC
   /**
    * Different build targets (Debug / Release etc) use different versions
    * of the 'C' runtime (which can't be 'mixed & matched'). Therefore, in
    * case the supplied search path contains multiple version(s) of a given
    * module, only select the one(s) which match the current build target
    */
	#if defined (_DEBUG)
		Glib::PatternSpec dll_extension_pattern("*D.dll");
	#elif defined (RDC_BUILD)
		Glib::PatternSpec dll_extension_pattern("*RDC.dll");
	#elif defined (_WIN64)
		Glib::PatternSpec dll_extension_pattern("*64.dll");
	#else
		Glib::PatternSpec dll_extension_pattern("*32.dll");
	#endif
#else
	Glib::PatternSpec dll_extension_pattern("*.dll");
#endif

	Glib::PatternSpec so_extension_pattern("*.so");
	Glib::PatternSpec dylib_extension_pattern("*.dylib");

	find_files_matching_pattern (cp_modules, control_protocol_search_path (),
	                             dll_extension_pattern);

	find_files_matching_pattern (cp_modules, control_protocol_search_path (),
	                             so_extension_pattern);

	find_files_matching_pattern (cp_modules, control_protocol_search_path (),
	                             dylib_extension_pattern);

	DEBUG_TRACE (DEBUG::ControlProtocols,
		     string_compose (_("looking for control protocols in %1\n"), control_protocol_search_path().to_string()));

	for (vector<std::string>::iterator i = cp_modules.begin(); i != cp_modules.end(); ++i) {
		control_protocol_discover (*i);
	}
}

int
ControlProtocolManager::control_protocol_discover (string path)
{
	ControlProtocolDescriptor* descriptor;

#ifdef __APPLE__
	/* don't load OS X shared objects that are just symlinks to the real thing.
	 */

	if (path.find (".dylib") && Glib::file_test (path, Glib::FILE_TEST_IS_SYMLINK)) {
		return 0;
	}
#endif

	if ((descriptor = get_descriptor (path)) != 0) {

		if (!descriptor->probe (descriptor)) {
			warning << string_compose (_("Control protocol %1 not usable"), descriptor->name) << endmsg;
		} else {

			ControlProtocolInfo* cpi = new ControlProtocolInfo ();

			cpi->descriptor = descriptor;
			cpi->name = descriptor->name;
			cpi->path = path;
			cpi->protocol = 0;
			cpi->requested = false;
			cpi->mandatory = descriptor->mandatory;
			cpi->supports_feedback = descriptor->supports_feedback;
			cpi->state = 0;

			control_protocol_info.push_back (cpi);

			DEBUG_TRACE (DEBUG::ControlProtocols,
				     string_compose(_("Control surface protocol discovered: \"%1\"\n"), cpi->name));
		}
	}

	return 0;
}

ControlProtocolDescriptor*
ControlProtocolManager::get_descriptor (string path)
{
	Glib::Module* module = new Glib::Module(path);
	ControlProtocolDescriptor *descriptor = 0;
	ControlProtocolDescriptor* (*dfunc)(void);
	void* func = 0;

	if (!(*module)) {
		error << string_compose(_("ControlProtocolManager: cannot load module \"%1\" (%2)"), path, Glib::Module::get_last_error()) << endmsg;
		delete module;
		return 0;
	}

	if (!module->get_symbol("protocol_descriptor", func)) {
		error << string_compose(_("ControlProtocolManager: module \"%1\" has no descriptor function."), path) << endmsg;
		error << Glib::Module::get_last_error() << endmsg;
		delete module;
		return 0;
	}

	dfunc = (ControlProtocolDescriptor* (*)(void))func;
	descriptor = dfunc();

	if (descriptor) {
		descriptor->module = (void*)module;
	}

	return descriptor;
}

void
ControlProtocolManager::foreach_known_protocol (boost::function<void(const ControlProtocolInfo*)> method)
{
	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
		method (*i);
	}
}

ControlProtocolInfo*
ControlProtocolManager::cpi_by_name (string name)
{
	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
		if (name == (*i)->name) {
			return *i;
		}
	}
	return 0;
}

int
ControlProtocolManager::set_state (const XMLNode& node, int /*version*/)
{
	XMLNodeList clist;
	XMLNodeConstIterator citer;

	Glib::Threads::Mutex::Lock lm (protocols_lock);

	clist = node.children();

	for (citer = clist.begin(); citer != clist.end(); ++citer) {
		XMLNode const * child = *citer;

		if (child->name() == X_("Protocol")) {

			bool active;
			std::string name;
			if (!child->get_property (X_("active"), active) ||
			    !child->get_property (X_("name"), name)) {
				continue;
			}

			ControlProtocolInfo* cpi = cpi_by_name (name);

			if (cpi) {
				delete cpi->state;
				cpi->state = new XMLNode (**citer);

				std::cerr << "protocol " << name << " active ? " << active << std::endl;

				if (active) {
					if (_session) {
						instantiate (*cpi);
					} else {
						cpi->requested = true;
					}
				} else {
					if (_session) {
						teardown (*cpi, true);
					} else {
						cpi->requested = false;
					}
				}
			} else {
				std::cerr << "protocol " << name << " not found\n";
			}
		}
	}

	return 0;
}

XMLNode&
ControlProtocolManager::get_state ()
{
	XMLNode* root = new XMLNode (state_node_name);
	Glib::Threads::Mutex::Lock lm (protocols_lock);

	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {

		if ((*i)->protocol) {
			XMLNode& child_state ((*i)->protocol->get_state());
			child_state.set_property (X_("active"), "yes");
			root->add_child_nocopy (child_state);
		} else if ((*i)->state) {
			XMLNode* child_state = new XMLNode (*(*i)->state);
			child_state->set_property (X_("active"), "no");
			root->add_child_nocopy (*child_state);
		} else {
			XMLNode* child_state = new XMLNode (X_("Protocol"));
			child_state->set_property (X_("name"), (*i)->name);
			child_state->set_property (X_("active"), "no");
			root->add_child_nocopy (*child_state);
		}

	}

	return *root;
}


ControlProtocolManager&
ControlProtocolManager::instance ()
{
	if (_instance == 0) {
		_instance = new ControlProtocolManager ();
	}

	return *_instance;
}

void
ControlProtocolManager::midi_connectivity_established ()
{
	Glib::Threads::Mutex::Lock lm (protocols_lock);

	for (list<ControlProtocol*>::iterator p = control_protocols.begin(); p != control_protocols.end(); ++p) {
		(*p)->midi_connectivity_established ();
	}
}

void
ControlProtocolManager::register_request_buffer_factories ()
{
	Glib::Threads::Mutex::Lock lm (protocols_lock);

	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {

		if ((*i)->descriptor == 0) {
			warning << string_compose (_("Control protocol \"%1\" has no descriptor"), (*i)->name) << endmsg;
			continue;
		}

		if ((*i)->descriptor->request_buffer_factory) {
			EventLoop::register_request_buffer_factory ((*i)->descriptor->name, (*i)->descriptor->request_buffer_factory);
		}
	}
}
