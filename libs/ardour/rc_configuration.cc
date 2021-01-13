/*
 * Copyright (C) 1999-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <unistd.h>
#include <cstdio> /* for snprintf, grrr */

#include <glib.h>
#include "pbd/gstdio_compat.h"
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/file_utils.h"
#include "pbd/replace_all.h"

#include "temporal/types_convert.h"

#include "ardour/audioengine.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/port.h"
#include "ardour/rc_configuration.h"
#include "ardour/session_metadata.h"
#include "ardour/transport_master_manager.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

/* this is global so that we do not have to indirect through an object pointer
   to reference it.
*/

namespace ARDOUR {
    float speed_quietning = 0.251189; // -12dB reduction for ffwd or rewind
}

static const char* user_config_file_name = "config";
static const char* system_config_file_name = "system_config";

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
      , _transport_master_state (0)
{
}

RCConfiguration::~RCConfiguration ()
{
	delete _control_protocol_state;
	delete _transport_master_state;
}

int
RCConfiguration::load_state ()
{
	std::string rcfile;
	GStatBuf statbuf;

	/* load system configuration first */

	if (find_file (ardour_config_search_path(), system_config_file_name, rcfile)) {

		/* stupid XML Parser hates empty files */

		if (g_stat (rcfile.c_str(), &statbuf)) {
			return -1;
		}

		if (statbuf.st_size != 0) {
			info << string_compose (_("Loading system configuration file %1"), rcfile) << endmsg;

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

	if (find_file (ardour_config_search_path(), user_config_file_name, rcfile)) {

		/* stupid XML parser hates empty files */

		if (g_stat (rcfile.c_str(), &statbuf)) {
			return -1;
		}

		if (statbuf.st_size != 0) {
			info << string_compose (_("Loading user configuration file %1"), rcfile) << endmsg;

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
	const std::string rcfile = Glib::build_filename (user_config_directory(), user_config_file_name);
	const std::string tmp = rcfile + temp_suffix;

	XMLTree tree;
	tree.set_root (&get_state());
	if (!tree.write (tmp.c_str())){
		error << string_compose (_("Config file %1 not saved"), rcfile) << endmsg;
		if (g_remove (tmp.c_str()) != 0) {
			error << string_compose(_("Could not remove temporary config file at path \"%1\" (%2)"), tmp, g_strerror (errno)) << endmsg;
		}
		return -1;
	}

	if (::g_rename (tmp.c_str(), rcfile.c_str()) != 0) {
		error << string_compose (_("Could not rename temporary config file %1 to %2 (%3)"), tmp, rcfile, g_strerror(errno)) << endmsg;
		if (g_remove (tmp.c_str()) != 0) {
			error << string_compose(_("Could not remove temporary config file at path \"%1\" (%2)"), tmp, g_strerror (errno)) << endmsg;
		}
		return -1;
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

	root = new XMLNode("Ardour");

	root->add_child_nocopy (get_variables ());

	root->add_child_nocopy (SessionMetadata::Metadata()->get_user_state());

	if (_extra_xml) {
		root->add_child_copy (*_extra_xml);
	}

	root->add_child_nocopy (ControlProtocolManager::instance().get_state());

	if (TransportMasterManager::exists()) {
		root->add_child_nocopy (TransportMasterManager::instance().get_state());
	}

	return *root;
}

XMLNode&
RCConfiguration::get_variables ()
{
	XMLNode* node;

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

	Stateful::save_extra_xml (root);

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		node = *niter;

		if (node->name() == "Config") {
			set_variables (*node);
		} else if (node->name() == "Metadata") {
			SessionMetadata::Metadata()->set_state (*node, version);
		} else if (node->name() == ControlProtocolManager::state_node_name) {
			_control_protocol_state = new XMLNode (*node);
		} else if (node->name() == TransportMasterManager::state_node_name) {
			_transport_master_state = new XMLNode (*node);
		}
	}

	DiskReader::set_chunk_samples (minimum_disk_read_bytes.get() / sizeof (Sample));
	DiskWriter::set_chunk_samples (minimum_disk_write_bytes.get() / sizeof (Sample));

	return 0;
}

void
RCConfiguration::set_variables (const XMLNode& node)
{
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(type,var,name,value) \
  if (var.set_from_node (node)) {            \
    ParameterChanged (name);                 \
  }

#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) \
  if (var.set_from_node (node)) {                            \
    ParameterChanged (name);                                 \
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

