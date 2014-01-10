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
#include <cstdlib>
#include <cstdio> /* for snprintf, grrr */

#include <glibmm/miscutils.h>

#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"
#include "pbd/file_utils.h"
#include "pbd/error.h"

#include "gtkmm2ext/rgb_macros.h"

#include "ardour/filesystem_paths.h"

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
	_dirty (false)
{
	load_state();
}

UIConfiguration::~UIConfiguration ()
{
}

void
UIConfiguration::map_parameters (boost::function<void (std::string)>& functor)
{
#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,Name,value) functor (Name);
#include "ui_config_vars.h"
#undef  UI_CONFIG_VARIABLE
}

int
UIConfiguration::load_defaults ()
{
	int found = 0;

	std::string default_ui_rc_file;
	std::string rcfile;

	if (getenv ("ARDOUR_SAE")) {
		rcfile = "ardour3_ui_sae.conf";
	} else {
		rcfile = "ardour3_ui_default.conf";
	}

	if (find_file_in_search_path (ardour_config_search_path(), rcfile, default_ui_rc_file) ) {
		XMLTree tree;
		found = 1;

		string rcfile = default_ui_rc_file;

		info << string_compose (_("Loading default ui configuration file %1"), rcfile) << endl;

		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("cannot read default ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root(), Stateful::loading_state_version)) {
			error << string_compose(_("default ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}

		_dirty = false;
	}

	return found;
}

int
UIConfiguration::load_state ()
{
	bool found = false;

	std::string default_ui_rc_file;

	if ( find_file_in_search_path (ardour_config_search_path(), "ardour3_ui_default.conf", default_ui_rc_file)) {
		XMLTree tree;
		found = true;

		string rcfile = default_ui_rc_file;

		info << string_compose (_("Loading default ui configuration file %1"), rcfile) << endl;

		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("cannot read default ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root(), Stateful::loading_state_version)) {
			error << string_compose(_("default ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}
	}

	std::string user_ui_rc_file;

	if (find_file_in_search_path (ardour_config_search_path(), "ardour3_ui.conf", user_ui_rc_file)) {
		XMLTree tree;
		found = true;

		string rcfile = user_ui_rc_file;

		info << string_compose (_("Loading user ui configuration file %1"), rcfile) << endmsg;

		if (!tree.read (rcfile)) {
			error << string_compose(_("cannot read ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root(), Stateful::loading_state_version)) {
			error << string_compose(_("user ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}

		_dirty = false;
	}

	if (!found)
		error << _("could not find any ui configuration file, canvas will look broken.") << endmsg;

	pack_canvasvars();

	return 0;
}

int
UIConfiguration::save_state()
{
	XMLTree tree;

	std::string rcfile(user_config_directory());
	rcfile = Glib::build_filename (rcfile, "ardour3_ui.conf");

	// this test seems bogus?
	if (rcfile.length()) {
		tree.set_root (&get_state());
		if (!tree.write (rcfile.c_str())){
			error << string_compose (_("Config file %1 not saved"), rcfile) << endmsg;
			return -1;
		}
	}

	_dirty = false;

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
UIConfiguration::set_state (const XMLNode& root, int /*version*/)
{
	if (root.name() != "Ardour") {
		return -1;
	}

	Stateful::save_extra_xml (root);

	XMLNodeList nlist = root.children();
	XMLNodeConstIterator niter;
	XMLNode *node;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		node = *niter;

		if (node->name() == "Canvas" ||  node->name() == "UI") {
			set_variables (*node);

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
#define CANVAS_VARIABLE(var,name) canvas_colors.insert (std::pair<std::string,UIConfigVariable<uint32_t>* >(name,&var));
#include "canvas_vars.h"
#undef  CANVAS_VARIABLE
}

uint32_t
UIConfiguration::color_by_name (const std::string& name)
{
	map<std::string,UIConfigVariable<uint32_t>* >::iterator i = canvas_colors.find (name);

	if (i != canvas_colors.end()) {
		return i->second->get();
	}

	// cerr << string_compose (_("Color %1 not found"), name) << endl;
	return RGBA_TO_UINT (g_random_int()%256,g_random_int()%256,g_random_int()%256,0xff);
}

void
UIConfiguration::set_dirty ()
{
	_dirty = true;
}

bool
UIConfiguration::dirty () const
{
	return _dirty;
}
