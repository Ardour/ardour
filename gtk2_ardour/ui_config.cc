/*
    Copyright (C) 1999-2014 Paul Davis

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
#include "pbd/stacktrace.h"

#include "gtkmm2ext/rgb_macros.h"

#include "ardour/filesystem_paths.h"

#include "ardour_ui.h"
#include "global_signals.h"
#include "ui_config.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourCanvas;

static const char* ui_config_file_name = "ui_config";
static const char* default_ui_config_file_name = "default_ui_config";
UIConfiguration* UIConfiguration::_instance = 0;

static const double hue_width = 18.0;

UIConfiguration::UIConfiguration ()
	:
#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,val) var (name,val),
#define CANVAS_STRING_VARIABLE(var,name) var (name),
#define CANVAS_FONT_VARIABLE(var,name) var (name),
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_STRING_VARIABLE
#undef  CANVAS_FONT_VARIABLE

	/* initialize all the base colors using default
	   colors for now. these will be reset when/if
	   we load the UI config file.
	*/

#undef CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,val) var (name,quantized (val)),
#include "base_colors.h"
#undef CANVAS_BASE_COLOR

	_dirty (false)
{
	_instance = this;

	/* pack all base colors into the configurable color map so that
	   derived colors can use them.
	*/
	  
#undef CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,color) configurable_colors.insert (make_pair (name,&var));
#include "base_colors.h"
#undef CANVAS_BASE_COLOR

#undef CANVAS_COLOR
#define CANVAS_COLOR(var,name,base,modifier) relative_colors.insert (make_pair (name, RelativeHSV (base,modifier)));
#include "colors.h"
#undef CANVAS_COLOR
	
#undef COLOR_ALIAS
#define COLOR_ALIAS(var,name,alias) color_aliases.insert (make_pair (name,alias));
#include "color_aliases.h"
#undef CANVAS_COLOR

	load_state();

	ARDOUR_UI_UTILS::ColorsChanged.connect (boost::bind (&UIConfiguration::color_theme_changed, this));
}

UIConfiguration::~UIConfiguration ()
{
}

UIConfiguration::RelativeHSV
UIConfiguration::color_as_relative_hsv (Color c)
{
	HSV variable (c);
	HSV closest;
	double shortest_distance = DBL_MAX;
	string closest_name;

	map<string,ColorVariable<Color>*>::iterator f;
	std::map<std::string,HSV> palette;

	for (f = configurable_colors.begin(); f != configurable_colors.end(); ++f) {
		palette.insert (make_pair (f->first, HSV (f->second->get())));
	}

	for (map<string,HSV>::iterator f = palette.begin(); f != palette.end(); ++f) {
		
		double d;
		HSV fixed (f->second);
		
		if (fixed.is_gray() || variable.is_gray()) {
			/* at least one is achromatic; HSV::distance() will do
			 * the right thing
			 */
			d = fixed.distance (variable);
		} else {
			/* chromatic: compare ONLY hue because our task is
			   to pick the HUE closest and then compute
			   a modifier. We want to keep the number of 
			   hues low, and by computing perceptual distance 
			   we end up finding colors that are to each
			   other without necessarily be close in hue.
			*/
			d = fabs (variable.h - fixed.h);
		}

		if (d < shortest_distance) {
			closest = fixed;
			closest_name = f->first;
			shortest_distance = d;
		}
	}
	
	/* we now know the closest color of the fixed colors to 
	   this variable color. Compute the HSV diff and
	   use it to redefine the variable color in terms of the
	   fixed one.
	*/
	
	HSV delta = variable.delta (closest);

	/* quantize hue delta so we don't end up with many subtle hues caused
	 * by original color choices
	 */

	delta.h = hue_width * (round (delta.h/hue_width));

	return RelativeHSV (closest_name, delta);
}

void
UIConfiguration::color_theme_changed ()
{
	return;
	
	map<std::string,RelativeHSV>::iterator current_color;

	/* we need to reset the quantized hues before we start, because
	 * otherwise when we call RelativeHSV::get() in color_compute()
	 * we don't get an answer based on the new base colors, but instead
	 * based on any existing hue quantization.
	 */

	for (current_color = relative_colors.begin(); current_color != relative_colors.end(); ++current_color) {
		current_color->second.quantized_hue = -1;
	}
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
        std::string rcfile;

	if (find_file (ardour_config_search_path(), default_ui_config_file_name, rcfile) ) {
		XMLTree tree;
		found = 1;

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

	std::string rcfile;

	if ( find_file (ardour_config_search_path(), default_ui_config_file_name, rcfile)) {
		XMLTree tree;
		found = true;

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

	if (find_file (ardour_config_search_path(), ui_config_file_name, rcfile)) {
		XMLTree tree;
		found = true;

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

	if (!found) {
		error << _("could not find any ui configuration file, canvas will look broken.") << endmsg;
	}

	return 0;
}

int
UIConfiguration::save_state()
{
	XMLTree tree;

	std::string rcfile(user_config_directory());
	rcfile = Glib::build_filename (rcfile, ui_config_file_name);

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

	node = new XMLNode (which_node);

#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_STRING_VARIABLE
#undef  CANVAS_FONT_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,Name,value) if (node->name() == "UI") { var.add_to_node (*node); }
#define CANVAS_STRING_VARIABLE(var,Name) if (node->name() == "Canvas") { var.add_to_node (*node); }
#define CANVAS_FONT_VARIABLE(var,Name) if (node->name() == "Canvas") { var.add_to_node (*node); }
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_STRING_VARIABLE
#undef  CANVAS_FONT_VARIABLE

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
#define UI_CONFIG_VARIABLE(Type,var,name,val) \
         if (var.set_from_node (node)) { \
		 ParameterChanged (name); \
		 }
#define CANVAS_STRING_VARIABLE(var,name)	\
         if (var.set_from_node (node)) { \
		 ParameterChanged (name); \
		 }
#define CANVAS_FONT_VARIABLE(var,name)	\
         if (var.set_from_node (node)) { \
		 ParameterChanged (name); \
		 }
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_STRING_VARIABLE
#undef  CANVAS_FONT_VARIABLE

	/* Reset base colors */

#undef  CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,val) \
	var.set_from_node (node);
#include "base_colors.h"
#undef CANVAS_BASE_COLOR	

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

ArdourCanvas::Color
UIConfiguration::base_color_by_name (const std::string& name) const
{
	map<std::string,ColorVariable<Color>* >::const_iterator i = configurable_colors.find (name);

	if (i != configurable_colors.end()) {
		return i->second->get();
	}

#if 0 // yet unsed experimental style postfix
	/* Idea: use identical colors but different font/sizes
	 * for variants of the same 'widget'.
	 *
	 * example:
	 *  set_name("mute button");  // in route_ui.cc
	 *  set_name("mute button small"); // in mixer_strip.cc
	 *
	 * ardour3_widget_list.rc:
	 *  widget "*mute button" style:highest "small_button"
	 *  widget "*mute button small" style:highest "very_small_text"
	 *
	 * both use color-schema of defined in
	 *   BUTTON_VARS(MuteButton, "mute button")
	 *
	 * (in this particular example the widgets should be packed
	 * vertically shinking the mixer strip ones are currently not)
	 */
	const size_t name_len = name.size();
	const size_t name_sep = name.find(':');
	for (i = configurable_colors.begin(); i != configurable_colors.end(), name_sep != string::npos; ++i) {
		const size_t cmp_len = i->first.size();
		const size_t cmp_sep = i->first.find(':');
		if (cmp_len >= name_len || cmp_sep == string::npos) continue;
		if (name.substr(name_sep) != i->first.substr(cmp_sep)) continue;
		if (name.substr(0, cmp_sep) != i->first.substr(0, cmp_sep)) continue;
		return i->second->get();
	}
#endif

	cerr << string_compose (_("Base Color %1 not found"), name) << endl;
	return RGBA_TO_UINT (g_random_int()%256,g_random_int()%256,g_random_int()%256,0xff);
}

ArdourCanvas::Color
UIConfiguration::color (const std::string& name) const
{
	map<string,string>::const_iterator e = color_aliases.find (name);

	if (e != color_aliases.end ()) {
		map<string,RelativeHSV>::const_iterator rc = relative_colors.find (e->second);
		if (rc != relative_colors.end()) {
			return rc->second.get();
		}
	} else {
		/* not an alias, try directly */
		map<string,RelativeHSV>::const_iterator rc = relative_colors.find (name);
		if (rc != relative_colors.end()) {
			return rc->second.get();
		}
	}
	
	cerr << string_compose (_("Color %1 not found"), name) << endl;
	
	return rgba_to_color ((g_random_int()%256)/255.0,
			      (g_random_int()%256)/255.0,
			      (g_random_int()%256)/255.0,
			      0xff);
}

ArdourCanvas::HSV
UIConfiguration::RelativeHSV::get() const
{
	HSV base (UIConfiguration::instance()->base_color_by_name (base_color));
	
	/* this operation is a little wierd. because of the way we originally
	 * computed the alpha specification for the modifiers used here
	 * we need to reset base's alpha to zero before adding the modifier.
	 */

	HSV self (base + modifier);
	
	if (quantized_hue >= 0.0) {
		self.h = quantized_hue;
	}
	
	return self;
}

Color
UIConfiguration::quantized (Color c) const
{
	HSV hsv (c);
	hsv.h = hue_width * (round (hsv.h/hue_width));
	return hsv.color ();
}

void
UIConfiguration::reset_relative (const string& name, const RelativeHSV& rhsv)
{
	RelativeColors::iterator i = relative_colors.find (name);
	if (i == relative_colors.end()) {
		return;
	}

	i->second = rhsv;
}

void
UIConfiguration::set_alias (string const & name, string const & alias)
{
	ColorAliases::iterator i = color_aliases.find (name);
	if (i == color_aliases.end()) {
		return;
	}

	i->second = alias;
	ARDOUR_UI_UTILS::ColorsChanged (); /* EMIT SIGNAL */
}
	
