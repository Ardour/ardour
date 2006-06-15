#include <dlfcn.h>

#include <pbd/compose.h>
#include <pbd/error.h>
#include <pbd/pathscanner.h>

#include <control_protocol/control_protocol.h>

#include <ardour/session.h>
#include <ardour/control_protocol_manager.h>




using namespace ARDOUR;
using namespace std;

#include "i18n.h"

ControlProtocolManager* ControlProtocolManager::_instance = 0;
const string ControlProtocolManager::state_node_name = X_("ControlProtocols");

ControlProtocolManager::ControlProtocolManager ()
{
	if (_instance == 0) {
		_instance = this;
	}

	_session = 0;
}

ControlProtocolManager::~ControlProtocolManager()
{
	Glib::Mutex::Lock lm (protocols_lock);

	for (list<ControlProtocol*>::iterator i = control_protocols.begin(); i != control_protocols.end(); ++i) {
		delete (*i);
	}

	control_protocols.clear ();
		
}

void
ControlProtocolManager::set_session (Session& s)
{
	_session = &s;
	_session->going_away.connect (mem_fun (*this, &ControlProtocolManager::drop_session));

	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
		if ((*i)->requested || (*i)->mandatory) {
			instantiate (**i);
			(*i)->requested = false;
		}
	}
}

void
ControlProtocolManager::drop_session ()
{
	_session = 0;

	{
		Glib::Mutex::Lock lm (protocols_lock);
		for (list<ControlProtocol*>::iterator p = control_protocols.begin(); p != control_protocols.end(); ++p) {
			delete *p;
		}
		control_protocols.clear ();
	}
}

ControlProtocol*
ControlProtocolManager::instantiate (ControlProtocolInfo& cpi)
{
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

	Glib::Mutex::Lock lm (protocols_lock);
	control_protocols.push_back (cpi.protocol);

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
		Glib::Mutex::Lock lm (protocols_lock);
		list<ControlProtocol*>::iterator p = find (control_protocols.begin(), control_protocols.end(), cpi.protocol);
		if (p != control_protocols.end()) {
			control_protocols.erase (p);
		}
	}
	
	cpi.protocol = 0;
	dlclose (cpi.descriptor->module);
	return 0;
}

static bool protocol_filter (const string& str, void *arg)
{
	/* Not a dotfile, has a prefix before a period, suffix is "so" */
	
	return str[0] != '.' && (str.length() > 3 && str.find (".so") == (str.length() - 3));
}

void
ControlProtocolManager::load_mandatory_protocols ()
{
	if (_session == 0) {
		return;
	}

	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
		if ((*i)->mandatory && ((*i)->protocol == 0)) {
			info << string_compose (_("Instantiating mandatory control protocol %1"), (*i)->name) << endmsg;
			instantiate (**i);
		}
	}
}

void
ControlProtocolManager::discover_control_protocols (string path)
{
	vector<string *> *found;
	PathScanner scanner;

	cerr << "looking for control protocols in " << path << endl;

	found = scanner (path, protocol_filter, 0, false, true);

	for (vector<string*>::iterator i = found->begin(); i != found->end(); ++i) {
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

		ControlProtocolInfo* cpi = new ControlProtocolInfo ();

		if (!descriptor->probe (descriptor)) {
			info << string_compose (_("Control protocol %1 not usable"), descriptor->name) << endmsg;
		} else {

			cpi->descriptor = descriptor;
			cpi->name = descriptor->name;
			cpi->path = path;
			cpi->protocol = 0;
			cpi->requested = false;
			cpi->mandatory = descriptor->mandatory;
			
			control_protocol_info.push_back (cpi);
			
			info << string_compose(_("Control surface protocol discovered: \"%1\""), cpi->name) << endmsg;
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
ControlProtocolManager::foreach_known_protocol (sigc::slot<void,const ControlProtocolInfo*> method)
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
ControlProtocolManager::set_state (const XMLNode& node)
{
	XMLNodeList clist;
	XMLNodeConstIterator citer;
	XMLProperty* prop;

	clist = node.children();

	for (citer = clist.begin(); citer != clist.end(); ++citer) {
		if ((*citer)->name() == X_("Protocol")) {
			prop = (*citer)->property (X_("active"));
			if (prop && prop->value() == X_("yes")) {
				if ((prop = (*citer)->property (X_("name"))) != 0) {
					ControlProtocolInfo* cpi = cpi_by_name (prop->value());
					if (cpi) {
						if (_session) {
							instantiate (*cpi);
						} else {
							cpi->requested = true;
						}
					}
				}
			}
		}    
	}
	return 0;
}

XMLNode&
ControlProtocolManager::get_state (void)
{
	XMLNode* root = new XMLNode (state_node_name);
	Glib::Mutex::Lock lm (protocols_lock);

	for (list<ControlProtocolInfo*>::iterator i = control_protocol_info.begin(); i != control_protocol_info.end(); ++i) {
		XMLNode* child = new XMLNode (X_("Protocol"));
		child->add_property (X_("name"), (*i)->name);
		child->add_property (X_("active"), (*i)->protocol ? "yes" : "no");
		root->add_child_nocopy (*child);
	}

	return *root;
}
