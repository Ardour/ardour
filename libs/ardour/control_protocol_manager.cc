#include <dlfcn.h>

#include <pbd/compose.h>
#include <pbd/error.h>
#include <pbd/pathscanner.h>

#include <ardour/control_protocol.h>
#include <ardour/control_protocol_manager.h>

using namespace ARDOUR;
using namespace PBD;
using namespace std;

#include "i18n.h"

ControlProtocolManager* ControlProtocolManager::_instance = 0;

ControlProtocolManager::ControlProtocolManager ()
{
	if (_instance == 0) {
		_instance = this;
	}
}

ControlProtocolManager::~ControlProtocolManager()
{
}

void
ControlProtocolManager::startup (Session& s)
{
	list<ControlProtocolInfo *>::iterator i;
	
	for (i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {

		ControlProtocolInfo* cpi = (*i);

		if (cpi->name == "Tranzport") {
		
			cpi->descriptor = get_descriptor ((*i)->path);
			
			if (cpi->descriptor == 0) {
				error << string_compose (_("control protocol name \"%1\" has no descriptor"), cpi->name) << endmsg;
				continue;
			}
			
			if ((cpi->protocol = cpi->descriptor->initialize (cpi->descriptor, &s)) == 0) {
				error << string_compose (_("control protocol name \"%1\" could not be initialized"), cpi->name) << endmsg;
				continue;
			}
			
			{
				LockMonitor lm (protocols_lock, __LINE__, __FILE__);
				control_protocols.push_back (cpi->protocol);
			}
			
			cerr << "start " << cpi->name << endl;
			cpi->protocol->init ();
			
			cerr << "activate " << cpi->name << endl;
			cpi->protocol->set_active (true);
			
			cerr << cpi->name << " now running\n";
		}
	}
}

ControlProtocol*
ControlProtocolManager::instantiate (Session& session, string name)
{
	list<ControlProtocolInfo *>::iterator i;

	for (i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
		if ((*i)->name == name) {
			break;
		}
	}

	if (i == control_protocol_info.end()) {
		error << string_compose (_("control protocol name \"%1\" is unknown"), name) << endmsg;
		return 0;
	}

	ControlProtocolInfo* cpi = (*i);

	cpi->descriptor = get_descriptor ((*i)->path);

	if (cpi->descriptor == 0) {
		error << string_compose (_("control protocol name \"%1\" has no descriptor"), name) << endmsg;
		return 0;
	}

	if ((cpi->protocol = cpi->descriptor->initialize (cpi->descriptor, &session)) == 0) {
		error << string_compose (_("control protocol name \"%1\" could not be initialized"), name) << endmsg;
		return 0;
	}

	LockMonitor lm (protocols_lock, __LINE__, __FILE__);
	control_protocols.push_back (cpi->protocol);
	return cpi->protocol;
}

int
ControlProtocolManager::teardown (string name)
{
	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
		ControlProtocolInfo* cpi = *i;

		if (cpi->name == name && cpi->descriptor && cpi->protocol) {
			cpi->descriptor->destroy (cpi->descriptor, cpi->protocol);
			
			{
				LockMonitor lm (protocols_lock, __LINE__, __FILE__);
				list<ControlProtocol*>::iterator p = find (control_protocols.begin(), control_protocols.end(), cpi->protocol);
				if (p != control_protocols.end()) {
					control_protocols.erase (p);
				}
			}

			cpi->protocol = 0;
			return 0;
		}
	}

	return -1;
}

static bool protocol_filter (const string& str, void *arg)
{
	/* Not a dotfile, has a prefix before a period, suffix is "so" */
	
	return str[0] != '.' && (str.length() > 3 && str.find (".so") == (str.length() - 3));
}

void
ControlProtocolManager::discover_control_protocols (string path)
{
	vector<string *> *found;
	PathScanner scanner;

	cerr << "CP Manager looking for surfaces\n";

	found = scanner (path, protocol_filter, 0, false, true);

	for (vector<string*>::iterator i = found->begin(); i != found->end(); ++i) {
		cerr << "CP Manager looking at " << **i << endl;
		control_protocol_discover (**i);
		delete *i;
	}

	delete found;
}

int
ControlProtocolManager::control_protocol_discover (string path)
{
	ControlProtocolDescriptor* descriptor;

	if ((descriptor = get_descriptor (path)) != 0) {

		ControlProtocolInfo* info = new ControlProtocolInfo ();

		info->descriptor = descriptor;
		info->name = descriptor->name;
		info->path = path;
		
		control_protocol_info.push_back (info);

		cerr << "Found \"" << info->name << "\"\n";

		dlclose (descriptor->module);

	} else {
		cerr << "no descriptor\n";
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
