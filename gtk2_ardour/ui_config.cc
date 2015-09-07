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

#if !defined USE_CAIRO_IMAGE_SURFACE && !defined NDEBUG
#define OPTIONAL_CAIRO_IMAGE_SURFACE
#endif

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <cstdlib>
#include <cstdio> /* for snprintf, grrr */

#include <cairo/cairo.h>

#include <glibmm/miscutils.h>
#include <glib/gstdio.h>

#include "pbd/convert.h"
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

	_dirty (false),
	aliases_modified (false),
	colors_modified (false),
	modifiers_modified (false),
	block_save (0)
{
	_instance = this;

	load_state();

	ARDOUR_UI_UTILS::ColorsChanged.connect (boost::bind (&UIConfiguration::colors_changed, this));

	ParameterChanged.connect (sigc::mem_fun (*this, &UIConfiguration::parameter_changed));
}

UIConfiguration::~UIConfiguration ()
{
}

void
UIConfiguration::colors_changed ()
{
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
		load_rc_file (true);
	} else if (param == "color-file") {
		load_color_theme ();
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
	
void
UIConfiguration::map_parameters (boost::function<void (std::string)>& functor)
{
#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,Name,value) functor (Name);
#include "ui_config_vars.h"
#undef  UI_CONFIG_VARIABLE
}

int
UIConfiguration::pre_gui_init ()
{
#ifdef CAIRO_SUPPORTS_FORCE_BUGGY_GRADIENTS_ENVIRONMENT_VARIABLE
	if (get_buggy_gradients()) {
		g_setenv ("FORCE_BUGGY_GRADIENTS", "1", 1);
	}
#endif
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	if (get_cairo_image_surface()) {
		g_setenv ("ARDOUR_IMAGE_SURFACE", "1", 1);
	}
#endif
	return 0;
}

UIConfiguration*
UIConfiguration::post_gui_init ()
{
	load_color_theme ();
	return this;
}

int
UIConfiguration::load_defaults ()
{
        std::string rcfile;
	int ret = -1;
	
	if (find_file (ardour_config_search_path(), default_ui_config_file_name, rcfile) ) {
		XMLTree tree;

		info << string_compose (_("Loading default ui configuration file %1"), rcfile) << endmsg;

		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("cannot read default ui configuration file \"%1\""), rcfile) << endmsg;
		} else {
			if (set_state (*tree.root(), Stateful::loading_state_version)) {
				error << string_compose(_("default ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			} else {
				_dirty = false;
				ret = 0;
			}
		}

	} else {
		warning << string_compose (_("Could not find default UI configuration file %1"), default_ui_config_file_name) << endmsg;
	}


	if (ret == 0) {
		/* reload color theme */
		load_color_theme (false);
		ARDOUR_UI_UTILS::ColorsChanged (); /* EMIT SIGNAL */
	}

	return ret;
}

int
UIConfiguration::load_color_theme (bool allow_own)
{
	std::string cfile;
	string basename;
	bool found = false;

	if (allow_own) {
		basename = "my-";
		basename += color_file.get();
		basename += ".colors";

		if (find_file (ardour_config_search_path(), basename, cfile)) {
			found = true;
		}
	}

	if (!found) {
		basename = color_file.get();
		basename += ".colors";
	
		if (find_file (ardour_config_search_path(), basename, cfile)) {
			found = true;
		}
	}

	if (found) {

		XMLTree tree;
		
		info << string_compose (_("Loading color file %1"), cfile) << endmsg;

		if (!tree.read (cfile.c_str())) {
			error << string_compose(_("cannot read color file \"%1\""), cfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root(), Stateful::loading_state_version)) {
			error << string_compose(_("color file \"%1\" not loaded successfully."), cfile) << endmsg;
			return -1;
		}

		ARDOUR_UI_UTILS::ColorsChanged ();
	} else {
		warning << string_compose (_("Color file %1 not found"), basename) << endmsg;
	}

	return 0;
}

int
UIConfiguration::store_color_theme ()
{
	XMLNode* root;
	LocaleGuard lg (X_("C"));

	root = new XMLNode("Ardour");

	XMLNode* parent = new XMLNode (X_("Colors"));
	for (Colors::const_iterator i = colors.begin(); i != colors.end(); ++i) {
		XMLNode* node = new XMLNode (X_("Color"));
		node->add_property (X_("name"), i->first);
		stringstream ss;
		ss << "0x" << setw (8) << setfill ('0') << hex << i->second;
		node->add_property (X_("value"), ss.str());
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

	parent = new XMLNode (X_("Modifiers"));
	for (Modifiers::const_iterator i = modifiers.begin(); i != modifiers.end(); ++i) {
		XMLNode* node = new XMLNode (X_("Modifier"));
		node->add_property (X_("name"), i->first);
		node->add_property (X_("modifier"), i->second.to_string());
		parent->add_child_nocopy (*node);
	}
	root->add_child_nocopy (*parent);

	XMLTree tree;
	std::string colorfile = Glib::build_filename (user_config_directory(), (string ("my-") + color_file.get() + ".colors"));
	
	tree.set_root (root);

	if (!tree.write (colorfile.c_str())){
		error << string_compose (_("Color file %1 not saved"), colorfile) << endmsg;
		return -1;
	}

	return 0;
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

	if (_dirty) {
		std::string rcfile = Glib::build_filename (user_config_directory(), ui_config_file_name);
		
		XMLTree tree;

		tree.set_root (&get_state());

		if (!tree.write (rcfile.c_str())){
			error << string_compose (_("Config file %1 not saved"), rcfile) << endmsg;
			return -1;
		}

		_dirty = false;
	}

	if (aliases_modified || colors_modified || modifiers_modified) {

		if (store_color_theme ()) {
			error << string_compose (_("Color file %1 not saved"), color_file.get()) << endmsg;
			return -1;
		}

		aliases_modified = false;
		colors_modified = false;
		modifiers_modified = false;
	}
	

	return 0;
}

XMLNode&
UIConfiguration::get_state ()
{
	XMLNode* root;
	LocaleGuard lg (X_("C"));

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
	LocaleGuard lg (X_("C"));

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
	/* this can load a generic UI configuration file or a colors file */

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

	XMLNode* colors = find_named_node (root, X_("Colors"));

	if (colors) {
		load_colors (*colors);
	}

	XMLNode* aliases = find_named_node (root, X_("ColorAliases"));

	if (aliases) {
		load_color_aliases (*aliases);
	}

	XMLNode* modifiers = find_named_node (root, X_("Modifiers"));

	if (modifiers) {
		load_modifiers (*modifiers);
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
UIConfiguration::load_colors (XMLNode const & node)
{
	XMLNodeList const nlist = node.children();
	XMLNodeConstIterator niter;
	XMLProperty const *name;
	XMLProperty const *color;
	
	colors.clear ();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() != X_("Color")) {
			continue;
		}
		name = (*niter)->property (X_("name"));
		color = (*niter)->property (X_("value"));

		if (name && color) {
			ArdourCanvas::Color c;
			c = strtoul (color->value().c_str(), 0, 16);
			colors.insert (make_pair (name->value(), c));
		}
	}
}

void
UIConfiguration::load_modifiers (XMLNode const & node)
{
	PBD::LocaleGuard lg ("C");
	XMLNodeList const nlist = node.children();
	XMLNodeConstIterator niter;
	XMLProperty const *name;
	XMLProperty const *mod;
	
	modifiers.clear ();
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() != X_("Modifier")) {
			continue;
		}

		name = (*niter)->property (X_("name"));
		mod = (*niter)->property (X_("modifier"));

		if (name && mod) {
			SVAModifier svam (mod->value());
			modifiers.insert (make_pair (name->value(), svam));
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
}

ArdourCanvas::SVAModifier
UIConfiguration::modifier (string const & name) const
{
	Modifiers::const_iterator m = modifiers.find (name);
	if (m != modifiers.end()) {
		return m->second;
	}
	return SVAModifier ();
}

ArdourCanvas::Color
UIConfiguration::color_mod (std::string const & colorname, std::string const & modifiername) const
{
	return HSV (color (colorname)).mod (modifier (modifiername)).color ();
}

ArdourCanvas::Color
UIConfiguration::color_mod (const ArdourCanvas::Color& color, std::string const & modifiername) const
{
	return HSV (color).mod (modifier (modifiername)).color ();
}

ArdourCanvas::Color
UIConfiguration::color (const std::string& name, bool* failed) const
{
	ColorAliases::const_iterator e = color_aliases.find (name);

	if (failed) {
		*failed = false;
	}
	
	if (e != color_aliases.end ()) {
		Colors::const_iterator rc = colors.find (e->second);
		if (rc != colors.end()) {
			return rc->second;
		}
	} else {
		/* not an alias, try directly */
		Colors::const_iterator rc = colors.find (name);
		if (rc != colors.end()) {
			return rc->second;
		}
	}
	
	if (!failed) {
		/* only show this message if the caller wasn't interested in
		   the fail status.
		*/
		cerr << string_compose (_("Color %1 not found"), name) << endl;
	}

	if (failed) {
		*failed = true;
	}
	
	return rgba_to_color ((g_random_int()%256)/255.0,
			      (g_random_int()%256)/255.0,
			      (g_random_int()%256)/255.0,
			      0xff);
}

Color
UIConfiguration::quantized (Color c) const
{
	HSV hsv (c);
	hsv.h = hue_width * (round (hsv.h/hue_width));
	return hsv.color ();
}

void
UIConfiguration::set_color (string const& name, ArdourCanvas::Color color)
{
	Colors::iterator i = colors.find (name);
	if (i == colors.end()) {
		return;
	}
	i->second = color;
	colors_modified = true;

	ARDOUR_UI_UTILS::ColorsChanged (); /* EMIT SIGNAL */
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
}

void
UIConfiguration::set_modifier (string const & name, SVAModifier svam)
{
	Modifiers::iterator m = modifiers.find (name);

	if (m == modifiers.end()) {
		return;
	}

	m->second = svam;
	modifiers_modified = true;

	ARDOUR_UI_UTILS::ColorsChanged (); /* EMIT SIGNAL */
}

void
UIConfiguration::load_rc_file (bool themechange, bool allow_own)
{
	string basename = ui_rc_file.get();
	std::string rc_file_path;

	if (!find_file (ardour_config_search_path(), basename, rc_file_path)) {
		warning << string_compose (_("Unable to find UI style file %1 in search path %2. %3 will look strange"),
                                           basename, ardour_config_search_path().to_string(), PROGRAM_NAME)
				<< endmsg;
		return;
	}

	info << "Loading ui configuration file " << rc_file_path << endmsg;

	Gtkmm2ext::UI::instance()->load_rcfile (rc_file_path, themechange);
}


