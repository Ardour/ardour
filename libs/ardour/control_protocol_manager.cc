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
#include "pbd/pthread_utils.h"

#ifdef COMPILER_MSVC
#define _WINSOCKAPI_
#endif

#include <libusb.h>
/* ControlProtocolManager is a singleton, so we can use static
 * here. This has the advantage that libusb.h does not need
 * to be used in ardour/control_protocol_manager.h which
 * is included by various UIs
 */
static libusb_hotplug_callback_handle _hpcp    = 0;
static libusb_context*                _usb_ctx = NULL;
static pthread_t                      _hotplug_thread;
static bool                           _hotplug_thread_run = false;
#endif

#include <glibmm/module.h>

#include <glibmm/fileutils.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"

#include "control_protocol/control_protocol.h"

#include "ardour/control_protocol_manager.h"
#include "ardour/debug.h"

#include "ardour/search_paths.h"
#include "ardour/selection.h"
#include "ardour/session.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

#include "pbd/i18n.h"

ControlProtocolManager* ControlProtocolManager::_instance       = 0;
const string            ControlProtocolManager::state_node_name = X_("ControlProtocols");

PBD::Signal<void (StripableNotificationListPtr)> ControlProtocolManager::StripableSelectionChanged;

/*
  Control Protocol lifecycle/lifetime management:

The ControlProtocolManager object is a singleton that is created upon demand,
and persists until program termination.

Modules (shared libs/DLLs) are discovered during global initialization of
libardour (ARDOUR::init()), via
ControlProtocolManager::discover_control_protocols(). This finds the files, and
creates a ControlProtocolInfo object for each file, along with finding and
calling the function in the module that provides the ControlProtocolDescriptor.

The list of ControlProtocolInfo objects is not changed from that point on,
until program termination.

ControlProtocolInfo objects provide add_instance(), remove_instance() and
remove_all_instances() methods. The first two are used to instantiate and
activate an instance of a ControlProtocol object via the function pointers
provided by a given module's descriptor. They also provide ::set_state() and
::get_state(). These two different aspects of the API are used as follows:

1. During application startup, no instances of any control protocls are
created. However, global state will be deserialized from the config file, and
is passed to the ControlProtocolManager where a copy is and retained.

2. As a session is loaded, if it has any state related to control protocols
(i.e. is not a new session), this will be passed to the ControlProtocolManager,
replacing the global state previously provided (which is deleted).

3. At the completion of session loading, ControlProtocolManager::use_state()
will be called, which will make appropriate calls to each ControlProtocolInfo's
::set_state() method, using the retained state from either #1 or #2 above. This
may instantiate/activate instances of various control protocols (or may do nothing).

4. During the lifetime of a session, the user may manually activate or
deactivate certain control protocols. Doing do will use the ::add_instance()
and ::remove_instance() methods.

5. The user may also connect hardware that has associated control surface
support. This will lead to calls to ControlProtocolManager's
::probe_usb_control_protocols() and ::probe_midi_control_protocols() methods,
which will in turn give ControlProtocolInfo objects that opportunity to
automatically instantiate the relevant instances of their module.

6. A call to each of these discovery mechanisms will also be made after
::use_state() has been called, to pick up any hardware that has been collected
since the last time ControlProtocolManager::set_state() was saved to disk.

7. After the combination of ::use_state() and the discovery calls has been made
(i.e. near the end of session loading). ControlProtocolManager::get_state() may
be called to save the current state to the global config file. This will not
override the state saved with the session, but will be used when creating new
sessions in the future.

When a session is destroyed, it tells the ControlProtocolManager to remove all
instances, which iterates over the list of ControlProtocolInfo objects and
makes the same request of them (deactivating and the destroying each instance
created from the module represented by that object).

Thus, in the period between session destruction and reloading (should that
occur), the ControlProtocolManager retains a list of ControlProtocolInfo
objects which described the modules found at startup, but there are no
instances of any control protocols.

The reason for this ordering is as follows: we cannot create most control
protocol instances until it is possible to create ports, and the set of control
surfaces that is enabled while a session is loaded is considered to be a
per-session setting. Consequently, we cannot instantiate control protocols
until the session is loaded and the engine is running, so that we have (a)
state (either from the global config file for new sessions, or the session file
for existing sessions) (b) the ability to create ports.

However, since we can also stop and restart the engine at arbitrary times, we
also need to repeat the calls to ::use_state() along with the discovery methods
whenever the engine restarts (with a session loaded).

THIS IS ACTUALLY MORE TRICKY THAN IT LOOKS, because port names for the same
hardware may change across engine starts (different engine, or other
things). We sort of just want to do "reestablish" connections and nothing more,
but that may not work.
*/


#ifdef HAVE_USB
static int
usb_hotplug_cb (libusb_context* ctx, libusb_device* device, libusb_hotplug_event event, void* user_data)
{
	ControlProtocolManager*         cpm = static_cast<ControlProtocolManager*> (user_data);
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

ControlProtocolInfo::ControlProtocolInfo (std::string const & pathstr)
	: automatic (false)
	, descriptor (get_descriptor (pathstr))
	, session (nullptr)
	, path (pathstr)
{
	if (!descriptor) {
		throw failed_constructor ();
	}

	name = descriptor->name;

	DEBUG_TRACE (DEBUG::ControlProtocols, string_compose ("created new CPI for %1 @ %2\n", name, path));
}

ControlProtocolInfo::~ControlProtocolInfo ()
{
	remove_all_instances ();

	if (descriptor) {
		delete (Glib::Module*) descriptor->module;
		/* cpi->descriptor is now inaccessible since dlclose() or equivalent
		 * has been performed, and the descriptor is (or could be) a static
		 * object made accessible by dlopen().
		 */
		descriptor = nullptr;
	}

	delete state;
	state = nullptr;

#ifdef HAVE_USB
	assert (!_hotplug_thread_run);
#endif
}

ControlProtocolDescriptor*
ControlProtocolInfo::get_descriptor (string path)
{
s	Glib::Module*              module     = new Glib::Module (path);
	ControlProtocolDescriptor* descriptor = 0;
	ControlProtocolDescriptor* (*dfunc) (void);
	void* func = 0;

	if (!(*module)) {
		error << string_compose (_("ControlProtocolInfo: cannot load module \"%1\" (%2)"), path, Glib::Module::get_last_error ()) << endmsg;
		delete module;
		return nullptr;
	}

	if (!module->get_symbol ("protocol_descriptor", func)) {
		error << string_compose (_("ControlProtocolInfo: module \"%1\" has no descriptor function."), path) << endmsg;
		error << Glib::Module::get_last_error () << endmsg;
		delete module;
		return nullptr;
	}

	dfunc      = (ControlProtocolDescriptor * (*)(void)) func;
	descriptor = dfunc ();

	if (descriptor) {
		descriptor->module = (void*) module;
	} else {
		delete module;
	}

	return descriptor;
}

XMLNode&
ControlProtocolInfo::get_state() const
{
	XMLNode* state (new XMLNode (X_("ControlProtocol")));

	state->set_property (X_("name"), name);

	PBD::RWLock::ReaderLock lm (instance_lock);

	for (auto const & inst : instances) {
		XMLNode& child_state (inst->get_state ());
		state->add_child_nocopy (child_state);
	}

	return *state;
}

int
ControlProtocolInfo::set_state (XMLNode const & node, int)
{
	if (node.name().empty()) {
		return 0;
	}

	remove_all_instances ();

	for (auto const & child : node.children()) {

		std::string device_name;

		if (!child->get_property ("device-name", device_name)) {
			continue;
		}

		ControlProtocol* cp = add_instance (*session, device_name);
		cp->set_state (*child, 0);
	}

	return 0;
}

ControlProtocol*
ControlProtocolInfo::get_instance (std::string device_name)
{
	if (instances.empty()) {
		return nullptr;
	}

	PBD::RWLock::ReaderLock lm (instance_lock);

	if (device_name.empty()) {
		assert (instances.size() == 0);
		return instances.front();
	}

	for (auto & inst : instances) {
		if (inst->device_name() == device_name) {
			return inst;
		}
	}

	return nullptr;
}

ControlProtocol*
ControlProtocolInfo::add_instance (Session& session, std::string device_name)
{
	ControlProtocol* cp = descriptor->initialize (&session, device_name);

	if (cp) {
		PBD::RWLock::WriterLock lm (instance_lock);
		instances.push_back (cp);
	}

	return cp;
}

bool
ControlProtocolInfo::remove_instance (ControlProtocol& instance)
{
	PBD::RWLock::WriterLock lm (instance_lock);

	for (auto ii = instances.begin(); ii != instances.end(); ++ii) {
		if (*ii == &instance) {
			descriptor->destroy (&instance);
			delete &instance;
			instances.erase (ii);
			return true;
		}
	}

	return false;
}

bool
ControlProtocolInfo::remove_instance (std::string device_name)
{
	{
		PBD::RWLock::ReaderLock lm (instance_lock);

		if (instances.empty()) {
			return false;
		}

		if (device_name.empty()) {
			assert (instances.size() == 1);
			/* let the loop below do the work */
		}
	}


	PBD::RWLock::WriterLock lm (instance_lock);
	ControlProtocol* to_be_deleted = nullptr;

	for (auto ii = instances.begin(); ii != instances.end(); ++ii) {
		if ((*ii)->device_name() == device_name) {
			to_be_deleted = *ii;
			instances.erase (ii);
			break;
		}
	}

	if (to_be_deleted) {
		descriptor->destroy (to_be_deleted);
		delete to_be_deleted;
	}

	return (bool) to_be_deleted;
}

void
ControlProtocolInfo::remove_all_instances ()
{
	std::vector<ControlProtocol*> to_be_deleted;

	{
		PBD::RWLock::ReaderLock lm (instance_lock);
		to_be_deleted = instances;
		instances.clear ();
	}

	for (auto & inst : to_be_deleted) {
		descriptor->destroy (inst);
		delete inst;
	}
}

bool
ControlProtocolInfo::active () const
{
	PBD::RWLock::ReaderLock lm (instance_lock);
	return !instances.empty();
}

bool
ControlProtocolInfo::is_active (std::string const & device_name)
{
	PBD::RWLock::ReaderLock lm (instance_lock);

	if (device_name.empty()) {
		assert (instances.size() == 1);
	}

	for (auto const & inst : instances) {
		if (inst->device_name() == device_name && inst->active()) {
			return true;
		}
	}
	return false;
}

void
ControlProtocolInfo::midi_connectivity_established (bool yn)
{
	PBD::RWLock::ReaderLock lm (instance_lock);

	for (auto & instance : instances) {
		instance->midi_connectivity_established (yn);
	}
}

void
ControlProtocolInfo::stripable_selection_changed (StripableNotificationListPtr sp)
{
	PBD::RWLock::ReaderLock lm (instance_lock);

	for (auto & instance : instances) {
		instance->stripable_selection_changed ();
	}
}

void
ControlProtocolInfo::set_session (Session& s)
{
	session = &s;
}

/* -------------------- */

ControlProtocolManager::ControlProtocolManager ()
	: pending_state (nullptr)
{
}

ControlProtocolManager::~ControlProtocolManager ()
{
	PBD::RWLock::WriterLock lm (_protocols_lock);

	for (auto const & cpi : _control_protocol_info) {
		delete cpi;
	}

	_control_protocol_info.clear ();
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

	{
		PBD::RWLock::ReaderLock lm (_protocols_lock);
		for (auto const & cpi : _control_protocol_info) {
			cpi->set_session (*s);
		}
	}

#ifdef HAVE_USB
	if (LIBUSB_SUCCESS == libusb_init (&_usb_ctx) && libusb_has_capability (LIBUSB_CAP_HAS_HOTPLUG)) {
		if (LIBUSB_SUCCESS == libusb_hotplug_register_callback (
		                          _usb_ctx,
		                          libusb_hotplug_event (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
		                          LIBUSB_HOTPLUG_ENUMERATE,
		                          LIBUSB_HOTPLUG_MATCH_ANY,
		                          LIBUSB_HOTPLUG_MATCH_ANY,
		                          LIBUSB_HOTPLUG_MATCH_ANY,
		                          usb_hotplug_cb, this,
		                          &_hpcp)) {
			_hotplug_thread_run = true;
			if (pthread_create_and_store ("Ctrl USB Hotplug", &_hotplug_thread, usb_hotplug_thread, this, 0)) {
				_hotplug_thread_run = false;
			}
		}
	}
#endif
}

int
ControlProtocolManager::activate (ControlProtocolInfo& cpi, std::string device_name)
{
	assert (_session);

	ControlProtocol*        cp;

	if (cpi.get_instance (device_name) != nullptr) {
		return 0;
	}

	if (!(cp = cpi.add_instance (*_session, device_name))) {
		return -1;
	}

	if (!cp->active()) {
		error << string_compose (_("Control protocol support for %1 (%2) failed to activate"), cpi.name, device_name) << endmsg;
		cpi.remove_instance (*cp);
	}

	return 0;
}

int
ControlProtocolManager::deactivate (ControlProtocolInfo& cpi, std::string device_name)
{
	if (!cpi.remove_instance (device_name)) {
		return -1;
	}

	ProtocolStatusChange (&cpi,  device_name, false); /* EMIT SIGNAL */

	return 0;
}


void
ControlProtocolManager::session_going_away ()
{
	SessionHandlePtr::session_going_away ();
	/* Session::destroy() will explicitly call disestablish so we don't
	 * have to worry about that here.
	 */
}

void
ControlProtocolManager::drop_protocols ()
{
	/* called explicitly by Session::destroy() so that we can clean up
	 * before the process cycle stops and ports vanish.
	 */

	PBD::RWLock::WriterLock lm (_protocols_lock);

	for (auto const& cpi : _control_protocol_info) {
		cpi->remove_all_instances ();
	}
}

void
ControlProtocolManager::remove_all_instances (ControlProtocolInfo& cpi)
{
	cpi.remove_all_instances ();
	ProtocolStatusChange (&cpi, std::string(), false);
}

struct ControlProtocolOrderByName {
	bool operator() (ControlProtocolInfo* const& a, ControlProtocolInfo* const& b) const
	{
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
#if defined(_DEBUG)
	Glib::PatternSpec dll_extension_pattern ("*D.dll");
#elif defined(RDC_BUILD)
	Glib::PatternSpec dll_extension_pattern ("*RDC.dll");
#elif defined(_WIN64)
	Glib::PatternSpec dll_extension_pattern ("*64.dll");
#else
	Glib::PatternSpec dll_extension_pattern ("*32.dll");
#endif
#else
	Glib::PatternSpec dll_extension_pattern ("*.dll");
#endif

	Glib::PatternSpec so_extension_pattern ("*.so");
	Glib::PatternSpec dylib_extension_pattern ("*.dylib");

	find_files_matching_pattern (cp_modules, control_protocol_search_path (),
	                             dll_extension_pattern);

	find_files_matching_pattern (cp_modules, control_protocol_search_path (),
	                             so_extension_pattern);

	find_files_matching_pattern (cp_modules, control_protocol_search_path (),
	                             dylib_extension_pattern);

	DEBUG_TRACE (DEBUG::ControlProtocols,
	             string_compose (_("found %1 control protocols in %1\n"), cp_modules.size(), control_protocol_search_path ().to_string ()));

	{
		PBD::RWLock::WriterLock lm (_protocols_lock);
		for (auto const & module_path : cp_modules) {


#ifdef __APPLE__
			/* don't load OS X shared objects that are just symlinks to the real thing.
			 */

			if (path.find (".dylib") && Glib::file_test (module_path, Glib::FILE_TEST_IS_SYMLINK)) {
				return 0;
			}
#endif

			try {
				ControlProtocolInfo* cpi = new ControlProtocolInfo (module_path);
				_control_protocol_info.push_back (cpi);
			} catch (...) {
				continue;
			}
		}

		ControlProtocolOrderByName cpn;
		_control_protocol_info.sort (cpn);
	}
}

ControlProtocolInfo*
ControlProtocolManager::cpi_by_name (string name)
{
	PBD::RWLock::ReaderLock lm (_protocols_lock);

	for (auto const& cpi : _control_protocol_info) {
		if (name == cpi->name) {
			return cpi;
		}
	}
	return nullptr;
}

int
ControlProtocolManager::set_state (const XMLNode& node, int)
{
	/* See comments on lifetime management at the top of this file to
	   understand this.
	*/

	if (pending_state) {
		delete pending_state;
	}

	pending_state = new XMLNode (node);

	return 0;
}

int
ControlProtocolManager::use_state ()
{
	assert (_session);
	assert (pending_state);

	XMLNodeList          clist;
	XMLNodeConstIterator citer;

	drop_protocols ();

	clist = pending_state->children ();

	for (citer = clist.begin (); citer != clist.end (); ++citer) {

		XMLNode const* child = *citer;

		if (child->name () == X_("Protocol")) {

			std::string name;

			if (!child->get_property (X_("name"), name)) {
				continue;
			}

			ControlProtocolInfo* cpi = cpi_by_name (name);

			if (!cpi) {
				std::cerr << "protocol " << name << " not found\n";
				continue;
			}

			cpi->set_state (*child, 0);
		}
	}

	return 0;
}

XMLNode&
ControlProtocolManager::get_state () const
{
	XMLNode*                root = new XMLNode (state_node_name);
	PBD::RWLock::ReaderLock lm (_protocols_lock);

	for (auto const & cpi : _control_protocol_info) {
		root->add_child_nocopy (cpi->get_state ());
	}

	return *root;
}

void
ControlProtocolManager::probe ()
{
	probe_midi_control_protocols ();
}

void
ControlProtocolManager::establish (Session& s)
{
	set_session (&s);

	use_state ();
	probe ();

	/* Catch up with session selection */

	CoreSelection::StripableAutomationControls sac;
	_session->selection ().get_stripables (sac);

	if (!sac.empty ()) {
		StripableNotificationListPtr v (new StripableNotificationList);
		for (CoreSelection::StripableAutomationControls::iterator i = sac.begin (); i != sac.end (); ++i) {
			if ((*i).stripable) {
				v->push_back (std::weak_ptr<Stripable> ((*i).stripable));
			}
		}
		if (!v->empty ()) {
			StripableSelectionChanged (v); /* EMIT SIGNAL */
		}
	}
}

void
ControlProtocolManager::disestablish ()
{
	/* called from Session::destroy() */
	drop_protocols ();
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
ControlProtocolManager::midi_connectivity_established (bool yn)
{
	PBD::RWLock::ReaderLock lm (_protocols_lock);

	for (auto const& cpi : _control_protocol_info) {
		cpi->midi_connectivity_established (yn);
	}
}

void
ControlProtocolManager::probe_midi_control_protocols ()
{
	if (!Config->get_auto_enable_surfaces ()) {
		return;
	}

	PBD::RWLock::ReaderLock lm (_protocols_lock);

	for (auto const& cpi : _control_protocol_info) {
		/* Note: manual teardown deletes the descriptor */
		PBD::RWLock::ReaderLock lm (_protocols_lock);
		if (!cpi->descriptor) {
			cpi->automatic = false;
			continue;
		}
		if (!cpi->descriptor->probe_port) {
			continue;
		}
		bool active = cpi->active();
		bool found  = cpi->descriptor->probe_port ();

		if (!active && found) {
			cpi->automatic = true;

			string device_name;

			if (cpi->instances.empty()) {
				auto en = cpi->descriptor->enumerate ();
				if (!en.empty()) {
					if (!en.begin()->second.empty()) {
						device_name = en.begin()->second.front();
					}
				}
			}
			lm.release();
			activate (*cpi, device_name);
		} else if (active && cpi->automatic && !found) {

			cpi->automatic = false;
			lm.release();
			deactivate (*cpi);
		}
	}
}

void
ControlProtocolManager::probe_usb_control_protocols (bool arrived, uint16_t vendor, uint16_t product)
{
	if (!Config->get_auto_enable_surfaces ()) {
		return;
	}

	for (auto const& cpi : _control_protocol_info) {

		if (!cpi->descriptor->match_usb || !cpi->descriptor->match_usb (vendor, product)) {
			continue;
		}

		bool active = cpi->active();

		if (!active && arrived) {
			cpi->automatic = true;
			activate (*cpi, {});
		} else if (active && cpi->automatic && !arrived) {
			cpi->automatic = false;
			deactivate (*cpi);
		}
	}
}

void
ControlProtocolManager::stripable_selection_changed (StripableNotificationListPtr sp)
{
	/* this sets up the (static) data structures owned by ControlProtocol
	   that are "shared" across all control protocols.
	*/

	DEBUG_TRACE (DEBUG::Selection, string_compose ("Surface manager: selection changed, now %1 stripables\n", sp ? sp->size () : -1));
	StripableSelectionChanged (sp); /* EMIT SIGNAL */

	/* now give each protocol the chance to respond to the selection change
	 */

	{
		PBD::RWLock::ReaderLock lm (_protocols_lock);

		for (auto const& cpi : _control_protocol_info) {
			DEBUG_TRACE (DEBUG::Selection, string_compose ("selection change notification for surface \"%1\"\n", cpi->name));
			cpi->stripable_selection_changed (sp);
		}
	}
}
