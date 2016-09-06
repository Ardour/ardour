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

#include "ardour/types.h" // required for opeators used in pbd/configuration_variable.h

#include "pbd/stateful.h"
#include "pbd/xml++.h"
#include "pbd/configuration_variable.h"

#include "canvas/colors.h"

#include "utils.h"

class UIConfiguration : public PBD::Stateful
{
private:
	UIConfiguration();
	~UIConfiguration();

    public:
	static UIConfiguration& instance();

	static std::string color_file_suffix;

	void load_rc_file (bool themechange, bool allow_own = true);

	int load_state ();
	int save_state ();
	int load_defaults ();
	int load_color_theme (bool allow_own);

	int set_state (const XMLNode&, int version);
	XMLNode& get_state (void);
	XMLNode& get_variables (std::string);
	void set_variables (const XMLNode&);

	std::string  color_file_name (bool use_my, bool with_version) const;

	typedef std::map<std::string,ArdourCanvas::Color> Colors;
	typedef std::map<std::string,std::string> ColorAliases;
	typedef std::map<std::string,ArdourCanvas::SVAModifier> Modifiers;

	Colors         colors;
	ColorAliases   color_aliases;
	Modifiers      modifiers;

	void set_alias (std::string const & name, std::string const & alias);
	void set_color (const std::string& name, ArdourCanvas::Color);
	void set_modifier (std::string const &, ArdourCanvas::SVAModifier svam);

	std::string color_as_alias (ArdourCanvas::Color c);
	ArdourCanvas::Color quantized (ArdourCanvas::Color) const;

	ArdourCanvas::Color color (const std::string&, bool* failed = 0) const;
	ArdourCanvas::Color color_mod (std::string const & color, std::string const & modifier) const;
	ArdourCanvas::Color color_mod (const ArdourCanvas::Color& color, std::string const & modifier) const;
	ArdourCanvas::HSV  color_hsv (const std::string&) const;
	ArdourCanvas::SVAModifier modifier (const std::string&) const;

	static std::string color_to_hex_string (ArdourCanvas::Color c);
	static std::string color_to_hex_string_no_alpha (ArdourCanvas::Color c);

	sigc::signal<void>  ColorsChanged;

	void reset_dpi ();

	float get_ui_scale ();

	sigc::signal<void>  DPIReset;

	sigc::signal<void,std::string> ParameterChanged;
	void map_parameters (boost::function<void (std::string)>&);

	void parameter_changed (std::string);

	/** called before initializing any part of the GUI. Sets up
	 *  any runtime environment required to make the GUI work
	 *  in specific ways.
	 */
	int pre_gui_init ();

	/** called after the GUI toolkit has been initialized.
	 */
	UIConfiguration* post_gui_init ();

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
#define UI_CONFIG_VARIABLE(Type,var,name,value) PBD::ConfigVariable<Type> var;
#include "ui_config_vars.h"
#undef UI_CONFIG_VARIABLE

#define CANVAS_FONT_VARIABLE(var,name) PBD::ConfigVariable<std::string> var;
#include "canvas_vars.h"
#undef CANVAS_FONT_VARIABLE

	XMLNode& state ();
	bool _dirty;
	bool aliases_modified;
	bool colors_modified;
	bool modifiers_modified;

	int  store_color_theme ();
	void load_color_aliases (XMLNode const &);
	void load_colors (XMLNode const &);
	void load_modifiers (XMLNode const &);
	void reset_gtk_theme ();
	int  load_color_file (std::string const &);
	void colors_changed ();

	uint32_t block_save;
};

#endif /* __ardour_ui_configuration_h__ */
