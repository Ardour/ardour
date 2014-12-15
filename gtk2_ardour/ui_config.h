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

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include "pbd/stateful.h"
#include "pbd/xml++.h"
#include "ardour/configuration_variable.h"

#include "canvas/colors.h"

#include "utils.h"

class UIConfiguration : public PBD::Stateful
{
    public:
	UIConfiguration();
	~UIConfiguration();

	static UIConfiguration* instance() { return _instance; }

	void load_rc_file (bool themechange, bool allow_own = true);

	int load_state ();
	int save_state ();
	int load_defaults ();

	int set_state (const XMLNode&, int version);
	XMLNode& get_state (void);
	XMLNode& get_variables (std::string);
	void set_variables (const XMLNode&);

	typedef std::map<std::string,ArdourCanvas::Color> Colors;
	typedef std::map<std::string,std::string> ColorAliases;
	typedef std::map<std::string,ArdourCanvas::SVAModifier> Modifiers;

	Colors         colors;
	ColorAliases   color_aliases;
	Modifiers      modifiers;

	void set_alias (std::string const & name, std::string const & alias);
	void set_color (const std::string& name, ArdourCanvas::Color);
	
	std::string color_as_alias (ArdourCanvas::Color c);
	ArdourCanvas::Color quantized (ArdourCanvas::Color) const;

	ArdourCanvas::Color color (const std::string&, bool* failed = 0) const;
	ArdourCanvas::Color color_mod (std::string const & color, std::string const & modifier) const;
	ArdourCanvas::HSV  color_hsv (const std::string&) const;
	ArdourCanvas::SVAModifier modifier (const std::string&) const;
		
        sigc::signal<void,std::string> ParameterChanged;
	void map_parameters (boost::function<void (std::string)>&);

	void parameter_changed (std::string);
	
#undef UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,value) \
	Type get_##var () const { return var.get(); } \
	bool set_##var (Type val) { bool ret = var.set (val); if (ret) { ParameterChanged (name); } return ret;  }
#include "ui_config_vars.h"
#undef  UI_CONFIG_VARIABLE
#define CANVAS_FONT_VARIABLE(var,name) \
	Pango::FontDescription get_##var () const { return ARDOUR_UI_UTILS::sanitized_font (var.get()); } \
	bool set_##var (const std::string& val) { bool ret = var.set (val); if (ret) { ParameterChanged (name); } return ret;  }
#include "canvas_vars.h"
#undef CANVAS_FONT_VARIABLE

  private:
	/* declare variables */

#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,value) ARDOUR::ConfigVariable<Type> var;
#include "ui_config_vars.h"
#undef UI_CONFIG_VARIABLE

#define CANVAS_FONT_VARIABLE(var,name) ARDOUR::ConfigVariable<std::string> var;
#include "canvas_vars.h"
#undef CANVAS_FONT_VARIABLE

	XMLNode& state ();
	bool _dirty;
	bool aliases_modified;
	bool colors_modified;
	
	static UIConfiguration* _instance;

	int store_color_theme ();
	void load_color_aliases (XMLNode const &);
	void load_colors (XMLNode const &);
	void load_modifiers (XMLNode const &);
	void reset_gtk_theme ();
	void colors_changed ();
	int load_color_theme (bool allow_own=true);

	uint32_t block_save;
};

#endif /* __ardour_ui_configuration_h__ */

