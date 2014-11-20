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

#include "pbd/stateful.h"
#include "pbd/xml++.h"
#include "ardour/configuration_variable.h"

#include "canvas/colors.h"

#include "utils.h"

/* This is very similar to ARDOUR::ConfigVariable but expects numeric values to
 * be in hexadecimal. This is because it is intended for use with color
 * specifications which are easier to scan for issues in "rrggbbaa" format than
 * as decimals.
 */
template<class T>
class ColorVariable : public ARDOUR::ConfigVariableBase
{
  public:
	ColorVariable (std::string str) : ARDOUR::ConfigVariableBase (str) {}
	ColorVariable (std::string str, T val) : ARDOUR::ConfigVariableBase (str), value (val) {}

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

	std::string get_as_string () const {
		std::stringstream ss;
		ss << std::hex;
		ss.fill('0');
		ss.width(8);
		ss << value;
		return ss.str ();
	}

	void set_from_string (std::string const & s) {
		std::stringstream ss;
		ss << std::hex;
		ss << s;
		ss >> value;
	}

  protected:
	T get_for_save() { return value; }
	T value;
};

class UIConfiguration : public PBD::Stateful
{
  public:
	UIConfiguration();
	~UIConfiguration();

	static UIConfiguration* instance() { return _instance; }

	std::map<std::string,ColorVariable<ArdourCanvas::Color> *> configurable_colors;

	bool dirty () const;
	void set_dirty ();

	int load_state ();
	int save_state ();
	int load_defaults ();

	int set_state (const XMLNode&, int version);
	XMLNode& get_state (void);
	XMLNode& get_variables (std::string);
	void set_variables (const XMLNode&);
	void pack_canvasvars ();
	void reset_derived_colors ();

	ArdourCanvas::Color base_color_by_name (const std::string&) const;
	ArdourCanvas::Color color (const std::string&) const;
	ArdourCanvas::HSV  color_hsv (const std::string&) const;

        sigc::signal<void,std::string> ParameterChanged;
	void map_parameters (boost::function<void (std::string)>&);

#undef UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,value) \
	Type get_##var () const { return var.get(); } \
	bool set_##var (Type val) { bool ret = var.set (val); if (ret) { ParameterChanged (name); } return ret;  }
#include "ui_config_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef CANVAS_STRING_VARIABLE
#define CANVAS_STRING_VARIABLE(var,name) \
	std::string get_##var () const { return var.get(); }			\
	bool set_##var (const std::string& val) { bool ret = var.set (val); if (ret) { ParameterChanged (name); } return ret;  }
#define CANVAS_FONT_VARIABLE(var,name) \
	Pango::FontDescription get_##var () const { return ARDOUR_UI_UTILS::sanitized_font (var.get()); } \
	bool set_##var (const std::string& val) { bool ret = var.set (val); if (ret) { ParameterChanged (name); } return ret;  }
#include "canvas_vars.h"
#undef CANVAS_STRING_VARIABLE
#undef CANVAS_FONT_VARIABLE

#undef CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,val) \
	ArdourCanvas::Color get_##var() const { return var.get(); } \
	bool set_##var (ArdourCanvas::Color v) { bool ret = var.set (v); if (ret) { ParameterChanged (#var); } return ret;  } \
	bool set_##var(const ArdourCanvas::HSV& v) const { return set_##var (v.color()); }
#include "base_colors.h"
#undef CANVAS_BASE_COLOR

#undef CANVAS_COLOR
#define CANVAS_COLOR(var,name,base,modifier) ArdourCanvas::Color get_##var() const { return var.get().color(); }
#include "colors.h"
#undef CANVAS_COLOR

  private:

	struct RelativeHSV {
		RelativeHSV (const std::string& b, const ArdourCanvas::HSV& mod) 
			: base_color (b)
			, modifier (mod)
			, quantized_hue (-1.0) {}
		std::string base_color;
		ArdourCanvas::HSV modifier;
		double quantized_hue;

		ArdourCanvas::HSV get() const;
        };

	/* these are loaded from serialized state (e.g. XML) */
	std::map<std::string,RelativeHSV> relative_colors;
	/* these are computed during color_compute()*/
	std::map<std::string,ArdourCanvas::HSV> actual_colors;
	/* these map from the name/key of relative colors to the color/value of actual colors */
	std::map<std::string,std::string> color_aliases;

	/* declare variables */

#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,value) ARDOUR::ConfigVariable<Type> var;
#include "ui_config_vars.h"
#undef UI_CONFIG_VARIABLE

#define CANVAS_STRING_VARIABLE(var,name) ARDOUR::ConfigVariable<std::string> var;
#define CANVAS_FONT_VARIABLE(var,name) ARDOUR::ConfigVariable<std::string> var;
#include "canvas_vars.h"
#undef CANVAS_STRING_VARIABLE
#undef CANVAS_FONT_VARIABLE

	/* declare base color variables (these are modifiable by the user) */

#undef CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,val) ColorVariable<ArdourCanvas::Color> var;
#include "base_colors.h"
#undef CANVAS_BASE_COLOR

	/* declare relative color variables (not directly modifiable) */

#undef CANVAS_COLOR
#define CANVAS_COLOR(var,name,base,modifier) RelativeHSV var;
#include "colors.h"
#undef CANVAS_COLOR

	XMLNode& state ();
	bool _dirty;
	static UIConfiguration* _instance;

	void color_compute ();
	void print_relative_def (std::string camelcase, std::string name, ArdourCanvas::Color c);
	void color_theme_changed ();
	void regenerate_relative_definitions ();
	ArdourCanvas::Color quantized (ArdourCanvas::Color);
};

#endif /* __ardour_ui_configuration_h__ */

