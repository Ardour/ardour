/*
    Copyright (C) 1999-2006 Paul Davis

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

#include <unistd.h>
#include <cstdio> /* for snprintf, grrr */

#include <glib.h>
#include <glib/gstdio.h> /* for g_stat() */
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/file_utils.h"

#include "midi++/manager.h"

#include "ardour/control_protocol_manager.h"
#include "ardour/diskstream.h"
#include "ardour/filesystem_paths.h"
#include "ardour/rc_configuration.h"
#include "ardour/session_metadata.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

/* this is global so that we do not have to indirect through an object pointer
   to reference it.
*/

namespace ARDOUR {
    float speed_quietning = 0.251189; // -12dB reduction for ffwd or rewind
}

RCConfiguration::RCConfiguration ()
	:
/* construct variables */
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(Type,var,name,value) var (name,value),
#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) var (name,value,mutator),
#include "ardour/rc_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
	_control_protocol_state (0)
{
}


RCConfiguration::~RCConfiguration ()
{
	for (list<XMLNode*>::iterator i = _midi_port_states.begin(); i != _midi_port_states.end(); ++i) {
		delete *i;
	}

	delete _control_protocol_state;
}

int
RCConfiguration::load_state ()
{
	std::string rcfile;
	struct stat statbuf;

	/* load system configuration first */

	if (find_file_in_search_path (ardour_config_search_path(), "ardour_system.rc", rcfile)) {

		/* stupid XML Parser hates empty files */

		if (g_stat (rcfile.c_str(), &statbuf)) {
			return -1;
		}

		if (statbuf.st_size != 0) {
			info << string_compose (_("Loading system configuration file %1"), rcfile) << endl;

			XMLTree tree;
			if (!tree.read (rcfile.c_str())) {
				error << string_compose(_("%1: cannot read system configuration file \"%2\""), PROGRAM_NAME, rcfile) << endmsg;
				return -1;
			}

			if (set_state (*tree.root(), Stateful::current_state_version)) {
				error << string_compose(_("%1: system configuration file \"%2\" not loaded successfully."), PROGRAM_NAME, rcfile) << endmsg;
				return -1;
			}
		} else {
			error << string_compose (_("Your system %1 configuration file is empty. This probably means that there was an error installing %1"), PROGRAM_NAME) << endmsg;
		}
	}

	/* now load configuration file for user */

	if (find_file_in_search_path (ardour_config_search_path(), "ardour.rc", rcfile)) {

		/* stupid XML parser hates empty files */

		if (g_stat (rcfile.c_str(), &statbuf)) {
			return -1;
		}

		if (statbuf.st_size != 0) {
			info << string_compose (_("Loading user configuration file %1"), rcfile) << endl;

			XMLTree tree;
			if (!tree.read (rcfile)) {
				error << string_compose(_("%1: cannot read configuration file \"%2\""), PROGRAM_NAME, rcfile) << endmsg;
				return -1;
			}

			if (set_state (*tree.root(), Stateful::current_state_version)) {
				error << string_compose(_("%1: user configuration file \"%2\" not loaded successfully."), PROGRAM_NAME, rcfile) << endmsg;
				return -1;
			}
		} else {
			warning << string_compose (_("your %1 configuration file is empty. This is not normal."), PROGRAM_NAME) << endmsg;
		}
	}

	return 0;
}

int
RCConfiguration::save_state()
{
	const std::string rcfile = Glib::build_filename (user_config_directory(), "ardour.rc");

	// this test seems bogus?
	if (!rcfile.empty()) {
		XMLTree tree;
		tree.set_root (&get_state());
		if (!tree.write (rcfile.c_str())){
			error << string_compose (_("Config file %1 not saved"), rcfile) << endmsg;
			return -1;
		}
	}

	return 0;
}

void
RCConfiguration::add_instant_xml(XMLNode& node)
{
	Stateful::add_instant_xml (node, user_config_directory ());
}

XMLNode*
RCConfiguration::instant_xml(const string& node_name)
{
	return Stateful::instant_xml (node_name, user_config_directory ());
}


XMLNode&
RCConfiguration::get_state ()
{
	XMLNode* root;
	LocaleGuard lg (X_("POSIX"));

	root = new XMLNode("Ardour");

        MIDI::Manager* mm = MIDI::Manager::instance();

        if (mm) {
		boost::shared_ptr<const MIDI::Manager::PortList> ports = mm->get_midi_ports();

                for (MIDI::Manager::PortList::const_iterator i = ports->begin(); i != ports->end(); ++i) {
                        root->add_child_nocopy((*i)->get_state());
                }
        }

	root->add_child_nocopy (get_variables ());

	root->add_child_nocopy (SessionMetadata::Metadata()->get_user_state());

	if (_extra_xml) {
		root->add_child_copy (*_extra_xml);
	}

	root->add_child_nocopy (ControlProtocolManager::instance().get_state());

	return *root;
}

XMLNode&
RCConfiguration::get_variables ()
{
	XMLNode* node;
	LocaleGuard lg (X_("POSIX"));

	node = new XMLNode ("Config");

#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(type,var,Name,value) \
	var.add_to_node (*node);
#define CONFIG_VARIABLE_SPECIAL(type,var,Name,value,mutator) \
	var.add_to_node (*node);
#include "ardour/rc_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

	return *node;
}

int
RCConfiguration::set_state (const XMLNode& root, int version)
{
	if (root.name() != "Ardour") {
		return -1;
	}

	XMLNodeList nlist = root.children();
	XMLNodeConstIterator niter;
	XMLNode *node;

	for (list<XMLNode*>::iterator i = _midi_port_states.begin(); i != _midi_port_states.end(); ++i) {
		delete *i;
	}

	_midi_port_states.clear ();

	Stateful::save_extra_xml (root);

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		node = *niter;

		if (node->name() == "Config") {
			set_variables (*node);
		} else if (node->name() == "Metadata") {
			SessionMetadata::Metadata()->set_state (*node, version);
		} else if (node->name() == ControlProtocolManager::state_node_name) {
			_control_protocol_state = new XMLNode (*node);
		} else if (node->name() == MIDI::Port::state_node_name) {
			_midi_port_states.push_back (new XMLNode (*node));
		}
	}

	Diskstream::set_disk_io_chunk_frames (minimum_disk_io_bytes.get() / sizeof (Sample));

	return 0;
}

void
RCConfiguration::set_variables (const XMLNode& node)
{
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(type,var,name,value) \
	if (var.set_from_node (node)) { \
		ParameterChanged (name);		  \
	}
#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) \
	if (var.set_from_node (node)) {    \
		ParameterChanged (name);		     \
	}

#include "ardour/rc_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

}
void
RCConfiguration::map_parameters (boost::function<void (std::string)>& functor)
{
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(type,var,name,value)                 functor (name);
#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) functor (name);
#include "ardour/rc_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
}
