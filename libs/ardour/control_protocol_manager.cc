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

#include <dlfcn.h>

#include <glibmm/fileutils.h>

#include "pbd/compose.h"
#include "pbd/file_utils.h"
#include "pbd/error.h"

#include "control_protocol/control_protocol.h"

#include "ardour/debug.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/control_protocol_search_path.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

#include "i18n.h"

ControlProtocolManager* ControlProtocolManager::_instance = 0;
const string ControlProtocolManager::state_node_name = X_("ControlProtocols");

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
				instantiate (**i);
				(*i)->requested = false;

				if ((*i)->protocol) {
					if ((*i)->state) {
						(*i)->protocol->set_state (*(*i)->state, Stateful::loading_state_version);
					} else {
						/* guarantee a call to
						   set_state() whether we have
						   existing state or not
						*/
						(*i)->protocol->set_state (XMLNode(""), Stateful::loading_state_version);
					}
				}
			}
		}
	}
}

void
ControlProtocolManager::session_going_away()
{
	SessionHandlePtr::session_going_away ();

	{
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
}

ControlProtocol*
ControlProtocolManager::instantiate (ControlProtocolInfo& cpi)
{
	/* CALLER MUST HOLD LOCK */

	if (_session == 0) {
		return 0;
	}

	cpi.descriptor = get_descriptor (cpi.path);

	if (cpi.descriptor == 0) {
		error << string_compose (_("control protocol name \"%1\" has no descriptor"), cpi.name) << endmsg;
		return 0;
	}

	if ((cpi.protocol = cpi.descriptor->initialize (cpi.descriptor, _session)) == 0) {
		error << string_compose (_("control protocol name \"%1\" could not be initialized"), cpi.name) << endmsg;
		return 0;
	}

	control_protocols.push_back (cpi.protocol);

	ProtocolStatusChange (&cpi);

	return cpi.protocol;
}

int
ControlProtocolManager::teardown (ControlProtocolInfo& cpi)
{
	if (!cpi.protocol) {
		return 0;
	}

	if (!cpi.descriptor) {
		return 0;
	}

	if (cpi.mandatory) {
		return 0;
	}

	cpi.descriptor->destroy (cpi.descriptor, cpi.protocol);

	{
		Glib::Threads::Mutex::Lock lm (protocols_lock);
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
	dlclose (cpi.descriptor->module);

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

	Glib::PatternSpec so_extension_pattern("*.so");
	Glib::PatternSpec dylib_extension_pattern("*.dylib");

	find_matching_files_in_search_path (control_protocol_search_path (),
					    so_extension_pattern, cp_modules);

	find_matching_files_in_search_path (control_protocol_search_path (),
					    dylib_extension_pattern, cp_modules);

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
			DEBUG_TRACE (DEBUG::ControlProtocols,
				     string_compose (_("Control protocol %1 not usable"), descriptor->name));
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
				     string_compose(_("Control surface protocol discovered: \"%1\""), cpi->name));
		}

		dlclose (descriptor->module);
	}

	return 0;
}

ControlProtocolDescriptor*
ControlProtocolManager::get_descriptor (string path)
{
	void *module;
	ControlProtocolDescriptor *descriptor = 0;
	ControlProtocolDescriptor* (*dfunc)(void);
	const char *errstr;

	if ((module = dlopen (path.c_str(), RTLD_NOW)) == 0) {
		error << string_compose(_("ControlProtocolManager: cannot load module \"%1\" (%2)"), path, dlerror()) << endmsg;
		return 0;
	}


	dfunc = (ControlProtocolDescriptor* (*)(void)) dlsym (module, "protocol_descriptor");

	if ((errstr = dlerror()) != 0) {
		error << string_compose(_("ControlProtocolManager: module \"%1\" has no descriptor function."), path) << endmsg;
		error << errstr << endmsg;
		dlclose (module);
		return 0;
	}

	descriptor = dfunc();
	if (descriptor) {
		descriptor->module = module;
	} else {
		dlclose (module);
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
	XMLProperty* prop;

	Glib::Threads::Mutex::Lock lm (protocols_lock);

	clist = node.children();

	for (citer = clist.begin(); citer != clist.end(); ++citer) {
		if ((*citer)->name() == X_("Protocol")) {

			if ((prop = (*citer)->property (X_("active"))) == 0) {
				continue;
			}

			bool active = string_is_affirmative (prop->value());
			
			if ((prop = (*citer)->property (X_("name"))) == 0) {
				continue;
			}

			ControlProtocolInfo* cpi = cpi_by_name (prop->value());
			
			if (cpi) {
				
				if (!(*citer)->children().empty()) {
					cpi->state = new XMLNode (*((*citer)->children().front ()));
				} else {
					cpi->state = 0;
				}
				
				if (active) {
					if (_session) {
						instantiate (*cpi);
					} else {
						cpi->requested = true;
					}
				} else {
					if (_session) {
						teardown (*cpi);
					} else {
						cpi->requested = false;
					}
				}
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

		XMLNode * child;

		if ((*i)->protocol) {
			child = &((*i)->protocol->get_state());
			child->add_property (X_("active"), "yes");
			// should we update (*i)->state here?  probably.
			root->add_child_nocopy (*child);
		} else if ((*i)->state) {
			// keep ownership clear
			root->add_child_copy (*(*i)->state);
		} else {
			child = new XMLNode (X_("Protocol"));
			child->add_property (X_("name"), (*i)->name);
			child->add_property (X_("active"), "no");
			root->add_child_nocopy (*child);
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
