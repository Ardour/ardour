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

    $Id$
*/

#include <unistd.h>
#include <cstdio> /* for snprintf, grrr */

#include <pbd/failed_constructor.h>
#include <pbd/xml++.h>

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/audio_diskstream.h>
#include <ardour/destructive_filesource.h>
#include <ardour/control_protocol_manager.h>

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

	user_configuration (false)
{
	_control_protocol_state = 0;
}

Configuration::~Configuration ()
{
}

int
Configuration::load_state ()
{
	string rcfile;
	
	/* load system configuration first */

	rcfile = find_config_file ("ardour_system.rc");

	if (rcfile.length()) {

		XMLTree tree;

		cerr << string_compose (_("loading system configuration file %1"), rcfile) << endl;
		
		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("Ardour: cannot read system configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root())) {
			error << string_compose(_("Ardour: system configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}
	}

	/* from this point on, all configuration changes are user driven */

	user_configuration = true;

	/* now load configuration file for user */
	
	rcfile = find_config_file ("ardour.rc");

	if (rcfile.length()) {

		XMLTree tree;
		
		cerr << string_compose (_("loading user configuration file %1"), rcfile) << endl;

		if (!tree.read (rcfile)) {
			error << string_compose(_("Ardour: cannot read configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root())) {
			error << string_compose(_("Ardour: user configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}
	}

	return 0;
}

int
Configuration::save_state()
{
	XMLTree tree;
	string rcfile;

	/* Note: this only writes the per-user file, and therefore
	   only saves variables marked as user-set or modified
	*/

	rcfile = get_user_ardour_path ();
	rcfile += "ardour.rc";

	if (rcfile.length()) {
		tree.set_root (&state (true));
		if (!tree.write (rcfile.c_str())){
			error << string_compose (_("Config file %1 not saved"), rcfile) << endmsg;
			return -1;
		}
	}

	return 0;
}

XMLNode&
Configuration::get_state ()
{
	return state (false);
}

XMLNode&
Configuration::state (bool user_only)
{
	XMLNode* root = new XMLNode("Ardour");
	LocaleGuard lg (X_("POSIX"));

	typedef map<string, MidiPortDescriptor*>::const_iterator CI;
	for(CI m = midi_ports.begin(); m != midi_ports.end(); ++m){
		root->add_child_nocopy(m->second->get_state());
	}

	XMLNode* node = new XMLNode("Config");
	
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	
#define CONFIG_VARIABLE(type,var,name,value) \
         if (!user_only || var.is_user()) var.add_to_node (*node);
#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) \
         if (!user_only || var.is_user()) var.add_to_node (*node);
#include "ardour/configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	

	root->add_child_nocopy (*node);

	if (_extra_xml) {
		root->add_child_copy (*_extra_xml);
	}

	root->add_child_nocopy (ControlProtocolManager::instance().get_state());

	return *root;
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
				pair<string,MidiPortDescriptor*> newpair;
				newpair.second = new MidiPortDescriptor (*node);
				newpair.first = newpair.second->tag;
				midi_ports.insert (newpair);
			}

			catch (failed_constructor& err) {
				warning << _("ill-formed MIDI port specification in ardour rcfile (ignored)") << endmsg;
			}

		} else if (node->name() == "Config") {

#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	
#define CONFIG_VARIABLE(type,var,name,value) \
         var.set_from_node (*node); \
  	 var.set_is_user (user_configuration);
#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) \
         var.set_from_node (*node); \
  	 var.set_is_user (user_configuration);
#include "ardour/configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	
			
		} else if (node->name() == "extra") {
			_extra_xml = new XMLNode (*node);

		} else if (node->name() == ControlProtocolManager::state_node_name) {
			_control_protocol_state = new XMLNode (*node);
		}
	}

	AudioDiskstream::set_disk_io_chunk_frames (minimum_disk_io_bytes.get() / sizeof (Sample));

	return 0;
}

Configuration::MidiPortDescriptor::MidiPortDescriptor (const XMLNode& node)
{
	const XMLProperty *prop;
	bool have_tag = false;
	bool have_device = false;
	bool have_type = false;
	bool have_mode = false;

	if ((prop = node.property ("tag")) != 0) {
		tag = prop->value();
		have_tag = true;
	}

	if ((prop = node.property ("device")) != 0) {
		device = prop->value();
		have_device = true;
	}

	if ((prop = node.property ("type")) != 0) {
		type = prop->value();
		have_type = true;
	}

	if ((prop = node.property ("mode")) != 0) {
		mode = prop->value();
		have_mode = true;
	}

	if (!have_tag || !have_device || !have_type || !have_mode) {
		throw failed_constructor();
	}
}

XMLNode&
Configuration::MidiPortDescriptor::get_state()
{
	XMLNode* root = new XMLNode("MIDI-port");

	root->add_property("tag", tag);
	root->add_property("device", device);
	root->add_property("type", type);
	root->add_property("mode", mode);

	return *root;
}

