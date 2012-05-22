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

#include <cmath>
#include <iostream>
#include <fstream>
#include <errno.h>

#include "fix_carbon.h"

#include <gtkmm/stock.h>
#include <gtkmm/settings.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/cell_renderer_color_selector.h"

#include "pbd/file_utils.h"

#include "ardour/configuration.h"
#include "ardour/filesystem_paths.h"
#include "ardour/profile.h"

#include "ardour_button.h"
#include "theme_manager.h"
#include "rgb_macros.h"
#include "ardour_ui.h"
#include "global_signals.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;

sigc::signal<void> ColorsChanged;
sigc::signal<void,uint32_t> ColorChanged;

ThemeManager::ThemeManager()
	: ArdourWindow (_("Theme Manager"))
	, dark_button (_("Dark Theme"))
	, light_button (_("Light Theme"))
	, reset_button (_("Restore Defaults"))
	, flat_buttons (_("Draw \"flat\" buttons"))
{
	set_title (_("Theme Manager"));

	color_list = TreeStore::create (columns);
	color_display.set_model (color_list);
	color_display.append_column (_("Object"), columns.name);

	Gtkmm2ext::CellRendererColorSelector* color_renderer = manage (new Gtkmm2ext::CellRendererColorSelector);
	TreeViewColumn* color_column = manage (new TreeViewColumn (_("Color"), *color_renderer));
	color_column->add_attribute (color_renderer->property_color(), columns.gdkcolor);

	color_display.append_column (*color_column);

	color_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	color_display.get_column (0)->set_expand (true);
	color_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	color_display.get_column (1)->set_expand (false);
	color_display.set_reorderable (false);
	color_display.get_selection()->set_mode (SELECTION_NONE);
	color_display.set_headers_visible (true);

	scroller.add (color_display);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	RadioButton::Group group = dark_button.get_group();
	light_button.set_group(group);
	theme_selection_hbox.set_homogeneous(false);
	theme_selection_hbox.pack_start (dark_button);
	theme_selection_hbox.pack_start (light_button);

	Gtk::VBox* vbox = Gtk::manage (new Gtk::VBox ());
	vbox->set_homogeneous (false);
	vbox->pack_start (theme_selection_hbox, PACK_SHRINK);
	vbox->pack_start (reset_button, PACK_SHRINK);
	vbox->pack_start (flat_buttons, PACK_SHRINK);
	vbox->pack_start (scroller);
	add (*vbox);

	color_display.signal_button_press_event().connect (sigc::mem_fun (*this, &ThemeManager::button_press_event), false);

	color_dialog.get_colorsel()->set_has_opacity_control (true);
	color_dialog.get_colorsel()->set_has_palette (true);

	color_dialog.get_ok_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_ACCEPT));
	color_dialog.get_cancel_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_CANCEL));
	dark_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_dark_theme_button_toggled));
	light_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_light_theme_button_toggled));
	reset_button.signal_clicked().connect (sigc::mem_fun (*this, &ThemeManager::reset_canvas_colors));
	flat_buttons.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_flat_buttons_toggled));

	set_size_request (-1, 400);
	setup_theme ();
}

ThemeManager::~ThemeManager()
{
}

int
ThemeManager::save (string /*path*/)
{
	return 0;
}

bool
ThemeManager::button_press_event (GdkEventButton* ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	UIConfigVariable<uint32_t> *ccvar;

	if (!color_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		/* allow normal processing to occur */
		return false;

	case 1: /* color */
		if ((iter = color_list->get_iter (path))) {

			UIConfigVariable<uint32_t>* var = (*iter)[columns.pVar];
			if (!var) {
				/* parent row, do nothing */
				return false;
			}

			int r,g, b, a;
			uint32_t rgba = (*iter)[columns.rgba];
			Gdk::Color color;

			UINT_TO_RGBA (rgba, &r, &g, &b, &a);
			color.set_rgb_p (r / 255.0, g / 255.0, b / 255.0);
			color_dialog.get_colorsel()->set_previous_color (color);
			color_dialog.get_colorsel()->set_current_color (color);
			color_dialog.get_colorsel()->set_previous_alpha (a * 256);
			color_dialog.get_colorsel()->set_current_alpha (a * 256);

			ResponseType result = (ResponseType) color_dialog.run();

			switch (result) {
			case RESPONSE_CANCEL:
				break;
			case RESPONSE_ACCEPT:
				color = color_dialog.get_colorsel()->get_current_color();
				a = color_dialog.get_colorsel()->get_current_alpha();
				r = (int) floor (color.get_red_p() * 255.0);
				g = (int) floor (color.get_green_p() * 255.0);
				b = (int) floor (color.get_blue_p() * 255.0);

				rgba = RGBA_TO_UINT(r,g,b,a>>8);
				(*iter)[columns.rgba] = rgba;
				(*iter)[columns.gdkcolor] = color;

				ccvar = (*iter)[columns.pVar];
				ccvar->set(rgba);
				ARDOUR_UI::config()->set_dirty ();

				ColorsChanged(); //EMIT SIGNAL
				break;

			default:
				break;

			}

			color_dialog.hide ();
		}
		return true;

	default:
		break;
	}

	return false;
}

void
load_rc_file (const string& filename, bool themechange)
{
	sys::path rc_file_path;

	if (!find_file_in_search_path (ardour_config_search_path(), filename, rc_file_path)) {
		warning << string_compose (_("Unable to find UI style file %1 in search path %2. %3 will look strange"),
                                           filename, ardour_config_search_path().to_string(), PROGRAM_NAME)
				<< endmsg;
		return;
	}

	info << "Loading ui configuration file " << rc_file_path.to_string() << endmsg;

	Gtkmm2ext::UI::instance()->load_rcfile (rc_file_path.to_string(), themechange);
}

/* hmm, this is a problem. the profile doesn't
   exist when the theme manager is constructed
   and toggles buttons during "normal" GTK setup.

   a better solution will be to make all Profile
   methods static or something.

   XXX FIX ME
*/

#define HACK_PROFILE_IS_SAE() (getenv("ARDOUR_SAE")!=0)

void
ThemeManager::on_flat_buttons_toggled ()
{
	ARDOUR_UI::config()->flat_buttons.set (flat_buttons.get_active());
	ARDOUR_UI::config()->set_dirty ();
	ArdourButton::set_flat_buttons (flat_buttons.get_active());
	/* force a redraw */
	gtk_rc_reset_styles (gtk_settings_get_default());
}

void
ThemeManager::on_dark_theme_button_toggled()
{
	if (!dark_button.get_active()) return;

	if (HACK_PROFILE_IS_SAE()){
		ARDOUR_UI::config()->ui_rc_file.set("ardour3_ui_dark_sae.rc");
	} else {
		ARDOUR_UI::config()->ui_rc_file.set("ardour3_ui_dark.rc");
	}
	ARDOUR_UI::config()->set_dirty ();

	load_rc_file (ARDOUR_UI::config()->ui_rc_file.get(), true);
}

void
ThemeManager::on_light_theme_button_toggled()
{
	if (!light_button.get_active()) return;

	if (HACK_PROFILE_IS_SAE()){
		ARDOUR_UI::config()->ui_rc_file.set("ardour3_ui_light_sae.rc");
	} else {
		ARDOUR_UI::config()->ui_rc_file.set("ardour3_ui_light.rc");
	}

	load_rc_file (ARDOUR_UI::config()->ui_rc_file.get(), true);
}

void
ThemeManager::setup_theme ()
{
	int r, g, b, a;

	color_list->clear();

	for (std::map<std::string,UIConfigVariable<uint32_t> *>::iterator i = ARDOUR_UI::config()->canvas_colors.begin(); i != ARDOUR_UI::config()->canvas_colors.end(); i++) {


		UIConfigVariable<uint32_t>* var = i->second;

		TreeModel::Children rows = color_list->children();
		TreeModel::Row row;
		string::size_type colon;

		if ((colon = var->name().find (':')) != string::npos) {

			/* this is supposed to be a child node, so find the
			 * parent 
			 */

			string parent = var->name().substr (0, colon);
			TreeModel::iterator ri;

			for (ri = rows.begin(); ri != rows.end(); ++ri) {
				string s = (*ri)[columns.name];
				if (s == parent) {
					break;
				}
			}

			if (ri == rows.end()) {
				/* not found, add the parent as new top level row */
				row = *(color_list->append());
				row[columns.name] = parent;
				row[columns.pVar] = 0;
				
				/* now add the child as a child of this one */

				row = *(color_list->insert (row->children().end()));
				row[columns.name] = var->name().substr (colon+1);
			} else {
				row = *(color_list->insert ((*ri)->children().end()));
				row[columns.name] = var->name().substr (colon+1);
			}

		} else {
			/* add as a child */
			row = *(color_list->append());
			row[columns.name] = var->name();
		}

		Gdk::Color col;
		uint32_t rgba = var->get();
		UINT_TO_RGBA (rgba, &r, &g, &b, &a);
		//cerr << (*i)->name() << " == " << hex << rgba << ": " << hex << r << " " << hex << g << " " << hex << b << endl;
		col.set_rgb_p (r / 255.0, g / 255.0, b / 255.0);

		row[columns.pVar] = var;
		row[columns.rgba] = rgba;
		row[columns.gdkcolor] = col;
	}

	ColorsChanged.emit();

	bool env_defined = false;
	string rcfile = Glib::getenv("ARDOUR3_UI_RC", env_defined);

	if(!env_defined) {
		rcfile = ARDOUR_UI::config()->ui_rc_file.get();
	}

	if (rcfile == "ardour3_ui_dark.rc" || rcfile == "ardour3_ui_dark_sae.rc") {
		dark_button.set_active();
	} else if (rcfile == "ardour3_ui_light.rc" || rcfile == "ardour3_ui_light_sae.rc") {
		light_button.set_active();
	}
	
	flat_buttons.set_active (ARDOUR_UI::config()->flat_buttons.get());
	
	load_rc_file(rcfile, false);
}

void
ThemeManager::reset_canvas_colors()
{
	ARDOUR_UI::config()->load_defaults();
	setup_theme ();
}

