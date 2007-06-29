/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __ardour_ui_configuration_h__
#define __ardour_ui_configuration_h__

#include <sstream>
#include <ostream>
#include <iostream>

#include <pbd/stateful.h> 
#include <pbd/xml++.h>

template<class T>
class UIConfigVariable 
{
  public:
	UIConfigVariable (std::string str) : _name (str) {}
	UIConfigVariable (std::string str, T val) : _name (str), value(val) {}

	std::string name() const { return _name; }

	bool set (T val) {
		if (val == value) {
			return false;
		}
		value = val;
		return true;
	}

	T get() const {
		return value;
	}

	void add_to_node (XMLNode& node) {
		std::stringstream ss;
		ss << std::hex;
		ss.fill('0');
		ss.width(8);
		ss << value;
		XMLNode* child = new XMLNode ("Option");
		child->add_property ("name", _name);
		child->add_property ("value", ss.str());
		node.add_child_nocopy (*child);
	}
	
	bool set_from_node (const XMLNode& node) {

		const XMLProperty* prop;
		XMLNodeList nlist;
		XMLNodeConstIterator niter;
		XMLNode* child;
			
		nlist = node.children();
			
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
				
			child = *niter;
				
			if (child->name() == "Option") {
				if ((prop = child->property ("name")) != 0) {
					if (prop->value() == _name) {
						if ((prop = child->property ("value")) != 0) {
							std::stringstream ss;
							ss << std::hex;
							ss << prop->value();
							ss >> value;

							return true;
						}
					}
				}
			}
		}
		return false;
	}

  protected:
	T get_for_save() { return value; }
	T value;
	std::string _name;
};

class UIConfiguration : public PBD::Stateful
{
  public:
	UIConfiguration();
	~UIConfiguration();

	std::vector<UIConfigVariable<uint32_t> *> canvas_colors;

	int load_state ();
	int save_state ();

	int set_state (const XMLNode&);
	XMLNode& get_state (void);
	XMLNode& get_variables (std::string);
	void set_variables (const XMLNode&);
	void pack_canvasvars ();

	sigc::signal<void,const char*> ParameterChanged;

#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,val) UIConfigVariable<Type> var;
#define CANVAS_VARIABLE(var,name) UIConfigVariable<uint32_t> var;
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_VARIABLE

  private:
	XMLNode& state ();
	bool hack;
};

#endif /* __ardour_ui_configuration_h__ */

