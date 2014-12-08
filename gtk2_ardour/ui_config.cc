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

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <cstdlib>
#include <cstdio> /* for snprintf, grrr */

#include <glibmm/miscutils.h>
#include <glib/gstdio.h>

#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"
#include "pbd/file_utils.h"
#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gtk_ui.h"

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
#define CANVAS_FONT_VARIABLE(var,name) var (name),
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_FONT_VARIABLE

	/* initialize all the base colors using default
	   colors for now. these will be reset when/if
	   we load the UI config file.
	*/

#undef CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,val) var (name,quantized (val)),
#include "base_colors.h"
#undef CANVAS_BASE_COLOR

	_dirty (false),
	aliases_modified (false),
	derived_modified (false),
	_saved_state_node (""),
	_saved_state_version (-1)
	
{
	_instance = this;

	/* pack all base colors into the configurable color map so that
	   derived colors can use them.
	*/
	  
#undef CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,color) base_colors.insert (make_pair (name,&var));
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

	ARDOUR_UI_UTILS::ColorsChanged.connect (boost::bind (&UIConfiguration::colors_changed, this));

	ParameterChanged.connect (sigc::mem_fun (*this, &UIConfiguration::parameter_changed));

	/* force GTK theme setting, so that loading an RC file will work */
	
	reset_gtk_theme ();
}

UIConfiguration::~UIConfiguration ()
{
}

void
UIConfiguration::colors_changed ()
{
	_dirty = true;

	reset_gtk_theme ();

	/* In theory, one of these ought to work:

	   gtk_rc_reparse_all_for_settings (gtk_settings_get_default(), true);
	   gtk_rc_reset_styles (gtk_settings_get_default());

	   but in practice, neither of them do. So just reload the current
	   GTK RC file, which causes a reset of all styles and a redraw
	*/

	parameter_changed ("ui-rc-file");
}

void
UIConfiguration::parameter_changed (string param)
{
	_dirty = true;
	
	if (param == "ui-rc-file") {
		load_rc_file (get_ui_rc_file(), true);
	}

	save_state ();
}

void
UIConfiguration::reset_gtk_theme ()
{
	stringstream ss;

	ss << "gtk_color_scheme = \"" << hex;
	
	for (ColorAliases::iterator g = color_aliases.begin(); g != color_aliases.end(); ++g) {
		
		if (g->first.find ("gtk_") == 0) {
			ColorAliases::const_iterator a = color_aliases.find (g->first);
			const string gtk_name = g->first.substr (4);
			ss << gtk_name << ":#" << std::setw (6) << setfill ('0') << (color (g->second) >> 8) << ';';
		}
	}

	ss << '"' << dec << endl;

	/* reset GTK color scheme */

	Gtk::Settings::get_default()->property_gtk_color_scheme() = ss.str();
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

	for (f = base_colors.begin(); f != base_colors.end(); ++f) {
		/* Do not include any specialized base colors in the palette
		   we use to do comparisons
		*/

		if (f->first.find ("color") == 0) {
			palette.insert (make_pair (f->first, HSV (f->second->get())));
		}
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

string
UIConfiguration::color_as_alias (Color c)
{
	string closest;
	double shortest_distance = DBL_MAX;
	HSV target (c);
	
	for (RelativeColors::const_iterator a = relative_colors.begin(); a != relative_colors.end(); ++a) {
		HSV hsv (a->second.get());
		double d = hsv.distance (target);
		if (d < shortest_distance) {
			shortest_distance = d;
			closest = a->first;
		}
	}
	return closest;
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

		info << string_compose (_("Loading default ui configuration file %1"), rcfile) << endmsg;

		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("cannot read default ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root(), Stateful::loading_state_version)) {
			error << string_compose(_("default ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}
		
		_dirty = false;

		ARDOUR_UI_UTILS::ColorsChanged ();
	} else {
		warning << string_compose (_("Could not find default UI configuration file %1"), default_ui_config_file_name) << endmsg;
	}

	return found;
}

int
UIConfiguration::load_state ()
{
	bool found = false;

	std::string rcfile;

	if (find_file (ardour_config_search_path(), default_ui_config_file_name, rcfile)) {
		XMLTree tree;
		found = true;

		info << string_compose (_("Loading default ui configuration file %1"), rcfile) << endmsg;

		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("cannot read default ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root(), Stateful::loading_state_version)) {
			error << string_compose(_("default ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}

		/* make a copy */
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

	ARDOUR_UI_UTILS::ColorsChanged ();

	return 0;
}

int
UIConfiguration::save_state()
{
	XMLTree tree;

	if (!dirty()) {
		return 0;
	}
	
	std::string rcfile(user_config_directory());
	rcfile = Glib::build_filename (rcfile, ui_config_file_name);

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

	XMLNode* parent = new XMLNode (X_("RelativeColors"));
	for (RelativeColors::const_iterator i = relative_colors.begin(); i != relative_colors.end(); ++i) {
		XMLNode* node = new XMLNode (X_("RelativeColor"));
		node->add_property (X_("name"), i->first);
		node->add_property (X_("base"), i->second.base_color);
		node->add_property (X_("modifier"), i->second.modifier.to_string());
		parent->add_child_nocopy (*node);
	}
	root->add_child_nocopy (*parent);

	
	parent = new XMLNode (X_("ColorAliases"));
	for (ColorAliases::const_iterator i = color_aliases.begin(); i != color_aliases.end(); ++i) {
		XMLNode* node = new XMLNode (X_("ColorAlias"));
		node->add_property (X_("name"), i->first);
		node->add_property (X_("alias"), i->second);
		parent->add_child_nocopy (*node);
	}
	root->add_child_nocopy (*parent);
	
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
#undef  CANVAS_FONT_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,Name,value) if (node->name() == "UI") { var.add_to_node (*node); }
#define CANVAS_FONT_VARIABLE(var,Name) if (node->name() == "Canvas") { var.add_to_node (*node); }
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
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

	XMLNode* relative = find_named_node (root, X_("RelativeColors"));
	
	if (relative) {
		load_relative_colors (*relative);
	}

	
	XMLNode* aliases = find_named_node (root, X_("ColorAliases"));

	if (aliases) {
		load_color_aliases (*aliases);
	}

	return 0;
}

void
UIConfiguration::load_color_aliases (XMLNode const & node)
{
	XMLNodeList const nlist = node.children();
	XMLNodeConstIterator niter;
	XMLProperty const *name;
	XMLProperty const *alias;
	
	color_aliases.clear ();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() != X_("ColorAlias")) {
			continue;
		}
		name = (*niter)->property (X_("name"));
		alias = (*niter)->property (X_("alias"));

		if (name && alias) {
			color_aliases.insert (make_pair (name->value(), alias->value()));
		}
	}
}

void
UIConfiguration::load_relative_colors (XMLNode const & node)
{
	XMLNodeList const nlist = node.children();
	XMLNodeConstIterator niter;
	XMLProperty const *name;
	XMLProperty const *base;
	XMLProperty const *modifier;
	
	relative_colors.clear ();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() != X_("RelativeColor")) {
			continue;
		}
		name = (*niter)->property (X_("name"));
		base = (*niter)->property (X_("base"));
		modifier = (*niter)->property (X_("modifier"));

		if (name && base && modifier) {
			RelativeHSV rhsv (base->value(), HSV (modifier->value()));
			relative_colors.insert (make_pair (name->value(), rhsv));
		}
	}

}

void
UIConfiguration::set_variables (const XMLNode& node)
{
#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,val) if (var.set_from_node (node)) { ParameterChanged (name); }
#define CANVAS_FONT_VARIABLE(var,name)        if (var.set_from_node (node)) { ParameterChanged (name); }
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_FONT_VARIABLE

	/* Reset base colors */

#undef  CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,val) var.set_from_node (node); /* we don't care about ParameterChanged here */
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
	return _dirty || aliases_modified || derived_modified;
}

ArdourCanvas::Color
UIConfiguration::base_color_by_name (const std::string& name) const
{
	map<std::string,ColorVariable<Color>* >::const_iterator i = base_colors.find (name);

	if (i != base_colors.end()) {
		return i->second->get();
	}

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
	derived_modified = true;

	ARDOUR_UI_UTILS::ColorsChanged (); /* EMIT SIGNAL */

	save_state ();
}

void
UIConfiguration::set_alias (string const & name, string const & alias)
{
	ColorAliases::iterator i = color_aliases.find (name);
	if (i == color_aliases.end()) {
		return;
	}

	i->second = alias;
	aliases_modified = true;

	ARDOUR_UI_UTILS::ColorsChanged (); /* EMIT SIGNAL */

	save_state ();
}

void
UIConfiguration::load_rc_file (const string& filename, bool themechange)
{
	std::string rc_file_path;

	if (!find_file (ardour_config_search_path(), filename, rc_file_path)) {
		warning << string_compose (_("Unable to find UI style file %1 in search path %2. %3 will look strange"),
                                           filename, ardour_config_search_path().to_string(), PROGRAM_NAME)
				<< endmsg;
		return;
	}

	info << "Loading ui configuration file " << rc_file_path << endmsg;

	Gtkmm2ext::UI::instance()->load_rcfile (rc_file_path, themechange);
}

std::ostream& operator<< (std::ostream& o, const UIConfiguration::RelativeHSV& rhsv)
{
	return o << rhsv.base_color << " + HSV(" << rhsv.modifier << ")";
}
