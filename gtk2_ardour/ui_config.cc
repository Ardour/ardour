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

#include <glibmm/miscutils.h>

#include <pbd/failed_constructor.h>
#include <pbd/xml++.h>
#include <pbd/error.h>

#include <ardour/ardour.h>

#include "ui_config.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

UIConfiguration::UIConfiguration ()
	:
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,val) var (name,val),
#define CANVAS_VARIABLE(var,name) var (name),
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_VARIABLE
	hack(true)
{
	load_state();
}

UIConfiguration::~UIConfiguration ()
{
}

int
UIConfiguration::load_defaults ()
{
	int found = 0;
	std::string rcfile;
	const char* ui_conf;

	ui_conf = getenv ("ARDOUR_UI_CONF");

	if (ui_conf && ui_conf[0] != '\0') {
		rcfile = find_config_file (ui_conf);
	} else {
		if (getenv ("ARDOUR_SAE")) {
			rcfile = find_config_file ("ardour2_ui_sae.conf");
		} else {
			rcfile = find_config_file ("ardour2_ui_default.conf");
		}
	}

	if (rcfile.length()) {

		XMLTree tree;
		found = 1;

		cerr << string_compose (_("loading default ui configuration file %1"), rcfile) << endl;
		
		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("Ardour: cannot read default ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root())) {
			error << string_compose(_("Ardour: default ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}
	}
	return found;
}
	
int
UIConfiguration::load_state ()
{
	int found = load_defaults ();
	std::string rcfile = find_config_file ("ardour2_ui.conf");

	if (rcfile.length())
	{
		XMLTree tree;
		found = true;

		cerr << string_compose (_("loading user ui configuration file %1"), rcfile) << endl;
		
		if (!tree.read (rcfile)) {
			error << string_compose(_("Ardour: cannot read ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root())) {
			error << string_compose(_("Ardour: user ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}
	}

	if (found != 1)
		error << "Ardour: could not find any ui configuration file, canvas will look broken." << endmsg;

	pack_canvasvars();
	return 0;
}

int
UIConfiguration::save_state()
{
	XMLTree tree;
	string rcfile;

	rcfile = Glib::build_filename (get_user_ardour_path (), "ardour2_ui.conf");

	if (rcfile.length()) {
		tree.set_root (&get_state());
		if (!tree.write (rcfile.c_str())){
			error << string_compose (_("UI config file %1 not saved"), rcfile) << endmsg;
			return -1;
		}
	}

	return 0;
}

XMLNode&
UIConfiguration::get_state ()
{
	XMLNode* root;
	LocaleGuard lg (X_("POSIX"));

	root = new XMLNode("Ardour");
	
	root->add_child_nocopy (get_variables ("UI"));
	root->add_child_nocopy (get_variables ("Canvas"));
	
	if (_extra_xml) {
		root->add_child_copy (*_extra_xml);
	}
	
	return *root;
}

XMLNode&
UIConfiguration::get_variables (std::string which_node)
{
	XMLNode* node;
	LocaleGuard lg (X_("POSIX"));

	node = new XMLNode(which_node);

#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,Name,value) if (node->name() == "UI") { var.add_to_node (*node); }
#define CANVAS_VARIABLE(var,Name) if (node->name() == "Canvas") { var.add_to_node (*node); }
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_VARIABLE

	return *node;
}

int
UIConfiguration::set_state (const XMLNode& root)
{
	if (root.name() != "Ardour") {
		return -1;
	}

	XMLNodeList nlist = root.children();
	XMLNodeConstIterator niter;
	XMLNode *node;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		node = *niter;
		if (node->name() == "Canvas" ||  node->name() == "UI") {
			set_variables (*node);

		} else if (node->name() == "extra") {
			_extra_xml = new XMLNode (*node);

		}
	}
	return 0;
}

void
UIConfiguration::set_variables (const XMLNode& node)
{
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,val) \
         if (var.set_from_node (node)) { \
		 ParameterChanged (name); \
		 }
#define CANVAS_VARIABLE(var,name) \
         if (var.set_from_node (node)) { \
		 ParameterChanged (name); \
		 }
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_VARIABLE
}

void
UIConfiguration::pack_canvasvars ()
{
#undef  CANVAS_VARIABLE
#define CANVAS_VARIABLE(var,name) canvas_colors.push_back(&var); 
#include "canvas_vars.h"
#undef  CANVAS_VARIABLE
}


