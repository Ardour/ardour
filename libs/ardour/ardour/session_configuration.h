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

#ifndef __ardour_session_configuration_h__
#define __ardour_session_configuration_h__

#include "ardour/configuration.h"

namespace ARDOUR {

class LIBARDOUR_API SessionConfiguration : public Configuration
{
public:
	SessionConfiguration ();

	void map_parameters (boost::function<void (std::string)>&);
	int set_state (XMLNode const &, int version);
	XMLNode& get_state ();
	XMLNode& get_variables ();
	void set_variables (XMLNode const &);

	/* define accessor methods */

#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(Type,var,name,value) \
	Type get_##var () const { return var.get(); } \
	bool set_##var (Type val) { bool ret = var.set (val); if (ret) { ParameterChanged (name); } return ret;  }
#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) \
	Type get_##var () const { return var.get(); } \
	bool set_##var (Type val) { bool ret = var.set (val); if (ret) { ParameterChanged (name); } return ret; }
#include "ardour/session_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

  private:

	/* declare variables */

#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(Type,var,name,value) ConfigVariable<Type> var;
#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) ConfigVariableWithMutation<Type> var;
#include "ardour/session_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

	int foo;
};

}

#endif
