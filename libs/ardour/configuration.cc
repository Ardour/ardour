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

#include <pbd/failed_constructor.h>
#include <pbd/xml++.h>
#include <pbd/filesystem.h>
#include <pbd/file_utils.h>

#include <midi++/manager.h>

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/audio_diskstream.h>
#include <ardour/control_protocol_manager.h>
#include <ardour/filesystem_paths.h>

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

Configuration::Configuration ()
	:
/* construct variables */
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	
#define CONFIG_VARIABLE(Type,var,name,value) var (name,value),
#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) var (name,value,mutator),
#include "ardour/configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL


	current_owner (ConfigVariableBase::Default)
{
	_control_protocol_state = 0;
}

Configuration::~Configuration ()
{
}

void
Configuration::set_current_owner (ConfigVariableBase::Owner owner)
{
	current_owner = owner;
}

int
Configuration::load_state ()
{
	bool found = false;

	sys::path system_rc_file;
	struct stat statbuf;

	/* load system configuration first */
	
	if ( find_file_in_search_path (ardour_search_path() + system_config_search_path(),
			"ardour_system.rc", system_rc_file) )
	{
		XMLTree tree;
		found = true;

		string rcfile = system_rc_file.to_string();

		/* stupid XML Parser hates empty files */
		
		if (g_stat (rcfile.c_str(), &statbuf)) {
			return -1;
		}

		if (statbuf.st_size != 0) {
			cerr << string_compose (_("loading system configuration file %1"), rcfile) << endl;
			
			if (!tree.read (rcfile.c_str())) {
				error << string_compose(_("Ardour: cannot read system configuration file \"%1\""), rcfile) << endmsg;
				return -1;
			}
			
			current_owner = ConfigVariableBase::System;
			
			if (set_state (*tree.root())) {
				error << string_compose(_("Ardour: system configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
				return -1;
			}
		} else {
			error << _("your system Ardour configuration file is empty. This probably means that there as an error installing Ardour") << endmsg;
		}
	}

	/* now load configuration file for user */

	sys::path user_rc_file;

	if (find_file_in_search_path (ardour_search_path() + user_config_directory(),
			"ardour.rc", user_rc_file))
	{
		XMLTree tree;
		
		string rcfile = user_rc_file.to_string();

		/* stupid XML parser hates empty files */

		if (g_stat (rcfile.c_str(), &statbuf)) {
			return -1;
		}

		if (statbuf.st_size != 0) {
			cerr << string_compose (_("loading user configuration file %1"), rcfile) << endl;
			
			if (!tree.read (rcfile)) {
				error << string_compose(_("Ardour: cannot read configuration file \"%1\""), rcfile) << endmsg;
				return -1;
			}
			
			current_owner = ConfigVariableBase::Config;
			
			if (set_state (*tree.root())) {
				error << string_compose(_("Ardour: user configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
				return -1;
			}
		} else {
			warning << _("your Ardour configuration file is empty. This is not normal.") << endmsg;
		}	
	}

	if (!found)
		error << "Ardour: could not find configuration file (ardour.rc), canvas will look broken." << endmsg;

	return 0;
}

int
Configuration::save_state()
{
	XMLTree tree;

	try
	{
		sys::create_directories (user_config_directory ());
	}
	catch (const sys::filesystem_error& ex)
	{
		error << "Could not create user configuration directory" << endmsg;
		return -1;
	}
	
	sys::path rcfile_path(user_config_directory());

	rcfile_path /= "ardour.rc";
	const string rcfile = rcfile_path.to_string();

	// this test seems bogus?
	if (rcfile.length()) {
		tree.set_root (&get_state());
		if (!tree.write (rcfile.c_str())){
			error << string_compose (_("Config file %1 not saved"), rcfile) << endmsg;
			return -1;
		}
	}

	return 0;
}

void
Configuration::add_instant_xml(XMLNode& node)
{
	Stateful::add_instant_xml (node, user_config_directory ());
}

XMLNode*
Configuration::instant_xml(const string& node_name)
{
	return Stateful::instant_xml (node_name, user_config_directory ());
}


bool
Configuration::save_config_options_predicate (ConfigVariableBase::Owner owner)
{
	/* only save things that were in the config file to start with */
	return owner & ConfigVariableBase::Config;
}

XMLNode&
Configuration::get_state ()
{
	XMLNode* root;
	LocaleGuard lg (X_("POSIX"));

	root = new XMLNode("Ardour");

	MIDI::Manager::PortMap::const_iterator i;
	const MIDI::Manager::PortMap& ports = MIDI::Manager::instance()->get_midi_ports();
	
	for (i = ports.begin(); i != ports.end(); ++i) {
		root->add_child_nocopy(i->second->get_state());
	}
	
	root->add_child_nocopy (get_variables (sigc::mem_fun (*this, &Configuration::save_config_options_predicate), "Config"));
	
	if (_extra_xml) {
		root->add_child_copy (*_extra_xml);
	}
	
	root->add_child_nocopy (ControlProtocolManager::instance().get_state());
	
	return *root;
}

XMLNode&
Configuration::get_variables (sigc::slot<bool,ConfigVariableBase::Owner> predicate, std::string which_node)
{
	XMLNode* node;
	LocaleGuard lg (X_("POSIX"));

	node = new XMLNode(which_node);

#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	
#define CONFIG_VARIABLE(type,var,Name,value) \
         if (node->name() == "Config") { if (predicate (var.owner())) { var.add_to_node (*node); }}
#define CONFIG_VARIABLE_SPECIAL(type,var,Name,value,mutator) \
         if (node->name() == "Config") { if (predicate (var.owner())) { var.add_to_node (*node); }}
#include "ardour/configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	

	return *node;
}

int
Configuration::set_state (const XMLNode& root)
{
	if (root.name() != "Ardour") {
		return -1;
	}

	XMLNodeList nlist = root.children();
	XMLNodeConstIterator niter;
	XMLNode *node;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		node = *niter;

		if (node->name() == "MIDI-port") {

			try {

				MIDI::Port::Descriptor desc (*node);
				map<string,XMLNode>::iterator x;
				if ((x = midi_ports.find (desc.tag)) != midi_ports.end()) {
					midi_ports.erase (x);
				}
				midi_ports.insert (pair<string,XMLNode>(desc.tag,*node));
			}

			catch (failed_constructor& err) {
				warning << _("ill-formed MIDI port specification in ardour rcfile (ignored)") << endmsg;
			}

		} else if (node->name() == "Config") {
			
			set_variables (*node, ConfigVariableBase::Config);
			
		} else if (node->name() == "extra") {
			_extra_xml = new XMLNode (*node);

		} else if (node->name() == ControlProtocolManager::state_node_name) {
			_control_protocol_state = new XMLNode (*node);
		}
	}

	Diskstream::set_disk_io_chunk_frames (minimum_disk_io_bytes.get() / sizeof (Sample));

	return 0;
}

void
Configuration::set_variables (const XMLNode& node, ConfigVariableBase::Owner owner)
{
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	
#define CONFIG_VARIABLE(type,var,name,value) \
         if (var.set_from_node (node, owner)) { \
		 ParameterChanged (name); \
	 }
#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) \
         if (var.set_from_node (node, owner)) { \
		 ParameterChanged (name); \
	 }

#include "ardour/configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
	
}
void
Configuration::map_parameters (sigc::slot<void,const char*> theSlot)
{
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	
#define CONFIG_VARIABLE(type,var,name,value)                 theSlot (name);
#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) theSlot (name);
#include "ardour/configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	
}

bool ConfigVariableBase::show_stores = false;

void
ConfigVariableBase::set_show_stored_values (bool yn)
{
	show_stores = yn;
}

void
ConfigVariableBase::show_stored_value (const string& str)
{
	if (show_stores) {
		cerr << "Config variable " << _name << " stored as " << str << endl;
	}
}

void
ConfigVariableBase::notify ()
{
	// placeholder for any debugging desired when a config variable is modified
}

void
ConfigVariableBase::miss ()
{
	// placeholder for any debugging desired when a config variable 
	// is set but to the same value as it already has
}

