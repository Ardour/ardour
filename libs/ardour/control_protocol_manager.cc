/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_USB
#include <libusb.h>
/* ControlProtocolManager is a singleton, so we can use static
 * here. This has the advantage that libusb.h does not need
 * to be used in ardour/control_protocol_manager.h which
 * is included by various UIs
 */
static libusb_hotplug_callback_handle _hpcp = 0;
static libusb_context*                _usb_ctx = NULL;
static pthread_t                      _hotplug_thread;
static bool                           _hotplug_thread_run = false;
#endif

#include <glibmm/module.h>

#include <glibmm/fileutils.h>

#include "pbd/compose.h"
#include "pbd/event_loop.h"
#include "pbd/file_utils.h"
#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "control_protocol/control_protocol.h"

#include "ardour/debug.h"
#include "ardour/control_protocol_manager.h"

#include "ardour/search_paths.h"
#include "ardour/selection.h"
#include "ardour/session.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

#include "pbd/i18n.h"

ControlProtocolManager* ControlProtocolManager::_instance = 0;
const string ControlProtocolManager::state_node_name = X_("ControlProtocols");
PBD::Signal1<void,StripableNotificationListPtr> ControlProtocolManager::StripableSelectionChanged;

#ifdef HAVE_USB
static int
usb_hotplug_cb (libusb_context* ctx, libusb_device* device, libusb_hotplug_event event, void* user_data)
{
	ControlProtocolManager* cpm = static_cast<ControlProtocolManager*> (user_data);
	struct libusb_device_descriptor desc;
	if (LIBUSB_SUCCESS == libusb_get_device_descriptor (device, &desc)) {
		DEBUG_TRACE (DEBUG::ControlProtocols, string_compose ("USB Hotplug: %1 vendor: %2 product: %3\n",
					(event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) ? "arrived" : "removed", std::hex, desc.idVendor, desc.idProduct));
		cpm->probe_usb_control_protocols (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, desc.idVendor, desc.idProduct);
	}

	return _hotplug_thread_run ? 0 : 1;
}

static void*
usb_hotplug_thread (void* user_data)
{
	while (_hotplug_thread_run) {
		if (libusb_handle_events (_usb_ctx) < 0) {
			break;
		}
	}
	return 0;
}
#endif

ControlProtocolInfo::~ControlProtocolInfo ()
{
	if (protocol && descriptor) {
		descriptor->destroy (protocol);
		protocol = 0;
	}

	delete state; state = 0;

	if (descriptor) {
		delete (Glib::Module*) descriptor->module;
		descriptor = 0;
	}
#ifdef HAVE_USB
	assert (!_hotplug_thread_run);
#endif
}

ControlProtocolManager::ControlProtocolManager ()
{
}

ControlProtocolManager::~ControlProtocolManager()
{
	Glib::Threads::RWLock::WriterLock lm (protocols_lock);

	for (list<ControlProtocol*>::iterator i = control_protocols.begin(); i != control_protocols.end(); ++i) {
		delete (*i);
	}

	control_protocols.clear ();


	for (list<ControlProtocolInfo*>::iterator p = control_protocol_info.begin(); p != control_protocol_info.end(); ++p) {
		(*p)->protocol = 0; // protocol was already destroyed above.
		delete (*p);
	}

	control_protocol_info.clear();
}

void
ControlProtocolManager::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
#ifdef HAVE_USB
		if (_hotplug_thread_run) {
			_hotplug_thread_run = false;
			libusb_hotplug_deregister_callback (_usb_ctx, _hpcp);
			pthread_join (_hotplug_thread, NULL);
		}
		if (_usb_ctx) {
			libusb_exit (_usb_ctx);
			_usb_ctx = NULL;
		}
#endif
		return;
	}

	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
		if ((*i)->requested) {
			(void) activate (**i);
		}
	}

	CoreSelection::StripableAutomationControls sac;
	_session->selection().get_stripables (sac);

	if (!sac.empty()) {
		StripableNotificationListPtr v (new StripableNotificationList);
		for (CoreSelection::StripableAutomationControls::iterator i = sac.begin(); i != sac.end(); ++i) {
			if ((*i).stripable) {
				v->push_back (std::weak_ptr<Stripable> ((*i).stripable));
			}
		}
		if (!v->empty()) {
			StripableSelectionChanged (v); /* EMIT SIGNAL */
		}
	}

#ifdef HAVE_USB
	if (LIBUSB_SUCCESS == libusb_init (&_usb_ctx) && libusb_has_capability (LIBUSB_CAP_HAS_HOTPLUG)) {
		if (LIBUSB_SUCCESS == libusb_hotplug_register_callback (_usb_ctx,
					libusb_hotplug_event(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
					LIBUSB_HOTPLUG_ENUMERATE,
					LIBUSB_HOTPLUG_MATCH_ANY,
					LIBUSB_HOTPLUG_MATCH_ANY,
					LIBUSB_HOTPLUG_MATCH_ANY,
					usb_hotplug_cb, this,
					&_hpcp)) {
			_hotplug_thread_run = true;
			if (pthread_create (&_hotplug_thread, NULL, usb_hotplug_thread, this)) {
				_hotplug_thread_run = false;
			}
		}
	}
#endif
}

int
ControlProtocolManager::activate (ControlProtocolInfo& cpi)
{
	ControlProtocol* cp;

	cpi.requested = true;

	if (cpi.protocol && cpi.protocol->active()) {
		warning << string_compose (_("Control protocol %1 was already active."), cpi.name) << endmsg;
		return 0;
	}

	Glib::Threads::RWLock::WriterLock lm (protocols_lock);
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

	Glib::Threads::RWLock::WriterLock lm (protocols_lock);

	for (list<ControlProtocolInfo*>::iterator p = control_protocol_info.begin(); p != control_protocol_info.end(); ++p) {
		// mark existing protocols as requested
		// otherwise the ControlProtocol instances are not recreated in set_session
		if ((*p)->protocol) {
			(*p)->requested = true;
			(*p)->protocol = 0;
			ProtocolStatusChange (*p); /* EMIT SIGNAL */
		}
	}

	for (list<ControlProtocol*>::iterator p = control_protocols.begin(); p != control_protocols.end(); ++p) {
		delete *p;
	}

	control_protocols.clear ();
}

ControlProtocol*
ControlProtocolManager::instantiate (ControlProtocolInfo& cpi)
{
	/* CALLER MUST HOLD LOCK */

	if (_session == 0) {
		return 0;
	}

	if (!cpi.descriptor) {
		cpi.descriptor = get_descriptor (cpi.path);
	}

	DEBUG_TRACE (DEBUG::ControlProtocols, string_compose ("instantiating %1\n", cpi.name));

	if (cpi.descriptor == 0) {
		error << string_compose (_("control protocol name \"%1\" has no descriptor"), cpi.name) << endmsg;
		return 0;
	}

	DEBUG_TRACE (DEBUG::ControlProtocols, string_compose ("initializing %1\n", cpi.name));

	if ((cpi.protocol = cpi.descriptor->initialize (_session)) == 0) {
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

	/* save current state */

	delete cpi.state;
	cpi.state = new XMLNode (cpi.protocol->get_state());
	cpi.state->set_property (X_("active"), false);

	cpi.descriptor->destroy (cpi.protocol);

	Glib::Threads::RWLock::WriterLock lm (protocols_lock, Glib::Threads::NOT_LOCK);
	if (lock_required) {
		/* the lock is required when the protocol is torn down by a user from the GUI. */
		lm.acquire ();
	}

	list<ControlProtocol*>::iterator p = find (control_protocols.begin(), control_protocols.end(), cpi.protocol);
	if (p != control_protocols.end()) {
		control_protocols.erase (p);
	} else {
		cerr << "Programming error: ControlProtocolManager::teardown() called for " << cpi.name << ", but it was not found in control_protocols" << endl;
	}

	if (lock_required) {
		lm.release();
	}

	cpi.protocol = 0;
	delete (Glib::Module*) cpi.descriptor->module;
	/* cpi->descriptor is now inaccessible since dlclose() or equivalent
	 * has been performed, and the descriptor is (or could be) a static
	 * object made accessible by dlopen().
	 */
	cpi.descriptor = 0;

	ProtocolStatusChange (&cpi);

	return 0;
}

struct ControlProtocolOrderByName
{
	bool operator() (ControlProtocolInfo* const & a, ControlProtocolInfo* const & b) const {
		return a->name < b->name;
	}
};

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

	ControlProtocolOrderByName cpn;
	control_protocol_info.sort (cpn);
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

		if (descriptor->available && !descriptor->available ()) {
			warning << string_compose (_("Control protocol %1 not usable"), descriptor->name) << endmsg;
			delete (Glib::Module*) descriptor->module;
		} else {

			ControlProtocolInfo* cpi = new ControlProtocolInfo ();

			cpi->descriptor = descriptor;
			cpi->name = descriptor->name;
			cpi->path = path;
			cpi->protocol = 0;
			cpi->requested = false;
			cpi->automatic = false;
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
	} else {
		delete module;
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
ControlProtocolManager::set_state (const XMLNode& node, int session_specific_state /* here: not version */)
{
	XMLNodeList clist;
	XMLNodeConstIterator citer;

	Glib::Threads::RWLock::WriterLock lm (protocols_lock);

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
				DEBUG_TRACE (DEBUG::ControlProtocols, string_compose ("Protocolstate %1 %2\n", name, active ? "active" : "inactive"));

				if (active) {
					delete cpi->state;
					cpi->state = new XMLNode (**citer);
					cpi->state->set_property (X_("session-state"), session_specific_state ? true : false);
					if (_session) {
						instantiate (*cpi);
					} else {
						cpi->requested = true;
					}
				} else {
					if (!cpi->state) {
						cpi->state = new XMLNode (**citer);
						cpi->state->set_property (X_("active"), false);
						cpi->state->set_property (X_("session-state"), session_specific_state ? true : false);
					}
					cpi->requested = false;
					if (_session) {
						teardown (*cpi, false);
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
ControlProtocolManager::get_state () const
{
	XMLNode* root = new XMLNode (state_node_name);
	Glib::Threads::RWLock::ReaderLock lm (protocols_lock);

	for (list<ControlProtocolInfo*>::const_iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {

		if ((*i)->protocol) {
			XMLNode& child_state ((*i)->protocol->get_state());
			child_state.set_property (X_("active"), !(*i)->automatic);
			delete ((*i)->state);
			(*i)->state = new XMLNode (child_state);
			root->add_child_nocopy (child_state);
		} else if ((*i)->state) {
			XMLNode* child_state = new XMLNode (*(*i)->state);
			child_state->set_property (X_("active"), false);
			root->add_child_nocopy (*child_state);
		} else {
			XMLNode* child_state = new XMLNode (X_("Protocol"));
			child_state->set_property (X_("name"), (*i)->name);
			child_state->set_property (X_("active"), false);
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
	Glib::Threads::RWLock::ReaderLock lm (protocols_lock);

	for (list<ControlProtocol*>::iterator p = control_protocols.begin(); p != control_protocols.end(); ++p) {
		(*p)->midi_connectivity_established ();
	}
}

void
ControlProtocolManager::probe_midi_control_protocols ()
{
	if (!Config->get_auto_enable_surfaces ()) {
		return;
	}
	for (auto const& cpi : control_protocol_info) {
		/* Note: manual teardown deletes the descriptor */
		if (!cpi->descriptor) {
			cpi->automatic = false;
			continue;
		}
		if (!cpi->descriptor->probe_port) {
			continue;
		}
		bool active = 0 != cpi->protocol;
		bool found = cpi->descriptor->probe_port ();

		if (!active && found) {
			cpi->automatic = true;
			activate (*cpi);
		} else if (active && cpi->automatic && !found) {
			cpi->automatic = false;
			deactivate (*cpi);
			/* allow to auto-enable again */
			if (!cpi->descriptor) {
				cpi->descriptor = get_descriptor (cpi->path);
			}
		}
	}
}

void
ControlProtocolManager::probe_usb_control_protocols (bool arrived, uint16_t vendor, uint16_t product)
{
	if (!Config->get_auto_enable_surfaces ()) {
		return;
	}
	for (auto const& cpi : control_protocol_info) {
		/* Note: manual teardown deletes the descriptor */
		if (!cpi->descriptor) {
			cpi->automatic = false;
			continue;
		}
		if (!cpi->descriptor->match_usb || !cpi->descriptor->match_usb (vendor, product)) {
			continue;
		}

		bool active = 0 != cpi->protocol;

		if (!active && arrived) {
			cpi->automatic = true;
			activate (*cpi);
		} else if (active && cpi->automatic && !arrived) {
			cpi->automatic = false;
			deactivate (*cpi);
			/* allow to auto-enable again */
			if (!cpi->descriptor) {
				cpi->descriptor = get_descriptor (cpi->path);
			}
		}
	}
}

void
ControlProtocolManager::stripable_selection_changed (StripableNotificationListPtr sp)
{
	/* this sets up the (static) data structures owned by ControlProtocol
	   that are "shared" across all control protocols.
	*/

	DEBUG_TRACE (DEBUG::Selection, string_compose ("Surface manager: selection changed, now %1 stripables\n", sp ? sp->size() : -1));
	StripableSelectionChanged (sp); /* EMIT SIGNAL */

	/* now give each protocol the chance to respond to the selection change
	 */

	{
		Glib::Threads::RWLock::ReaderLock lm (protocols_lock);

		for (list<ControlProtocol*>::iterator p = control_protocols.begin(); p != control_protocols.end(); ++p) {
			DEBUG_TRACE (DEBUG::Selection, string_compose ("selection change notification for surface \"%1\"\n", (*p)->name()));
			(*p)->stripable_selection_changed ();
		}
	}
}
