/*
    Copyright (C) 2009 Paul Davis

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

#include "ardour/types.h"
#include "ardour/utils.h"
#include "ardour/session_configuration.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

SessionConfiguration::SessionConfiguration ()
	:
/* construct variables */
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(Type,var,name,value) var (name,value),
#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) var (name,value,mutator),
#include "ardour/session_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
	foo (0)
{

}

XMLNode&
SessionConfiguration::get_state ()
{
	XMLNode* root;
	LocaleGuard lg (X_("POSIX"));

	root = new XMLNode ("Ardour");
	root->add_child_nocopy (get_variables ());

	return *root;
}


XMLNode&
SessionConfiguration::get_variables ()
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
#include "ardour/session_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

	return *node;
}


int
SessionConfiguration::set_state (XMLNode const& root, int /*version*/)
{
	if (root.name() != "Ardour") {
		return -1;
	}

	for (XMLNodeConstIterator i = root.children().begin(); i != root.children().end(); ++i) {
		if ((*i)->name() == "Config") {
			set_variables (**i);
		}
	}

	return 0;
}

void
SessionConfiguration::set_variables (const XMLNode& node)
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

#include "ardour/session_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

}
void
SessionConfiguration::map_parameters (boost::function<void (std::string)>& functor)
{
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(type,var,name,value)                 functor (name);
#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) functor (name);
#include "ardour/session_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
}
