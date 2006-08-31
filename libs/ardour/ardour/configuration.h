/*
    Copyright (C) 1999 Paul Davis 

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

#ifndef __ardour_configuration_h__
#define __ardour_configuration_h__

#include <map>
#include <vector>

#include <sys/types.h>
#include <string>

#include <pbd/stateful.h> 

#include <ardour/types.h>
#include <ardour/utils.h>
#include <ardour/configuration_variable.h>

class XMLNode;

namespace ARDOUR {

class Configuration : public Stateful
{
  public:
	Configuration();
	virtual ~Configuration();

	struct MidiPortDescriptor {
	    std::string tag;
	    std::string device;
	    std::string type;
	    std::string mode;

	    MidiPortDescriptor (const XMLNode&);
	    XMLNode& get_state();
	};

	std::map<std::string,MidiPortDescriptor *> midi_ports;

	int load_state ();
	int save_state ();

	int set_state (const XMLNode&);
	XMLNode& get_state (void);

	XMLNode* control_protocol_state () { return _control_protocol_state; }

	sigc::signal<void,const char*> ParameterChanged;

        /* define accessor methods */

#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(Type,var,name,value) \
        Type get_##var () const { return var.get(); } \
        void set_##var (Type val) { var.set (val); var.set_is_user (user_configuration); ParameterChanged (name); }
#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) \
        Type get_##var () const { return var.get(); } \
        void set_##var (Type val) { var.set (val); var.set_is_user (user_configuration); ParameterChanged (name); }
#include "ardour/configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

	
  private:

        /* declare variables */

#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	
#define CONFIG_VARIABLE(Type,var,name,value) ConfigVariable<Type> var;
#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) ConfigVariableWithMutation<Type> var;
#include "ardour/configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL	

	bool     user_configuration;
	XMLNode* _control_protocol_state;

	XMLNode& state (bool user_only);
};

extern Configuration *Config;
extern gain_t speed_quietning; /* see comment in configuration.cc */

} // namespace ARDOUR

#endif /* __ardour_configuration_h__ */
