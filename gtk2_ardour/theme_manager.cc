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

#include <gtkmm/stock.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm/settings.h>

#include <pbd/file_utils.h>

#include <ardour/configuration.h>
#include <ardour/filesystem_paths.h>

#include "theme_manager.h"
#include "rgb_macros.h"
#include "ardour_ui.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;


sigc::signal<void> ColorsChanged;
sigc::signal<void,uint32_t> ColorChanged;

ThemeManager::ThemeManager()
	: ArdourDialog ("ThemeManager"),
	dark_button ("Dark Theme"),
	light_button ("Light Theme")
{
	color_list = ListStore::create (columns);
	color_display.set_model (color_list);
	color_display.append_column (_("Object"), columns.name);
	color_display.append_column (_("Color"), columns.color);
	color_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));	
	color_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));	
	color_display.set_reorderable (false);
	color_display.get_selection()->set_mode (SELECTION_NONE);
	color_display.set_headers_visible (true);

	CellRenderer* color_cell = color_display.get_column_cell_renderer (1);
	TreeViewColumn* color_column = color_display.get_column (1);
	color_column->add_attribute (color_cell->property_cell_background_gdk(), columns.gdkcolor);
	
	scroller.add (color_display);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	
	RadioButton::Group group = dark_button.get_group();
	light_button.set_group(group);
	theme_selection_hbox.set_homogeneous(false);
	theme_selection_hbox.pack_start (dark_button);
	theme_selection_hbox.pack_start (light_button);

	get_vbox()->set_homogeneous(false);
	get_vbox()->pack_start (theme_selection_hbox, PACK_SHRINK);
	get_vbox()->pack_start (scroller);

	color_display.signal_button_press_event().connect (mem_fun (*this, &ThemeManager::button_press_event), false);

	color_dialog.get_colorsel()->set_has_opacity_control (true);
	color_dialog.get_colorsel()->set_has_palette (true);

	color_dialog.get_ok_button()->signal_clicked().connect (bind (mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_ACCEPT));
	color_dialog.get_cancel_button()->signal_clicked().connect (bind (mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_CANCEL));
	dark_button.signal_clicked().connect (bind (mem_fun (*this, &ThemeManager::load_rc), 1));
	light_button.signal_clicked().connect (bind (mem_fun (*this, &ThemeManager::load_rc), 2));

	set_size_request (-1, 400);
}

ThemeManager::~ThemeManager()
{
}

int
ThemeManager::save (string path)
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

	ARDOUR::ConfigVariable<uint32_t> *ccvar;
	
	if (!color_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		/* allow normal processing to occur */
		return false;

	case 1: /* color */
		if ((iter = color_list->get_iter (path))) {

			int r,g, b, a;
			uint32_t rgba = (*iter)[columns.rgba];
			Gdk::Color color;

			UINT_TO_RGBA (rgba, &r, &g, &b, &a);
			color.set_rgb_p (r / 255.0, g / 255.0, b / 255.0);
			color_dialog.get_colorsel()->set_current_color (color);
			color_dialog.get_colorsel()->set_previous_color (color);
			color_dialog.get_colorsel()->set_current_alpha (a * 256);
			color_dialog.get_colorsel()->set_previous_alpha (a * 256);
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

				rgba = RGBA_TO_UINT(r,g,b,a);
				//cerr << (*iter)[columns.name] << " == " << hex << rgba << endl;
				(*iter)[columns.rgba] = rgba;
				(*iter)[columns.gdkcolor] = color;

				ccvar = (*iter)[columns.pVar];
				ccvar->set(rgba);

				//ColorChanged (rgba);
				ColorsChanged();//EMIT SIGNAL
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
ThemeManager::load_rc(int which)
{
		
	if (which == 1) {
		Config->set_ui_rc_file("ardour2_ui_dark.rc");
		cerr << "dark theme selected" << endl;
		
	} else {
		Config->set_ui_rc_file("ardour2_ui_light.rc");
		cerr << "light theme selected" << endl;
	}
	
	sys::path rc_file_path;

	find_file_in_search_path (ardour_search_path() + system_config_search_path(),
			Config->get_ui_rc_file(), rc_file_path);

	ThemeChanged(rc_file_path.to_string()); //EMIT SIGNAL
	
	cerr << "load_rc() called " << rc_file_path.to_string() << endl;

}

void
ThemeManager::setup_theme ()
{
	int r, g, b, a;
	for (std::vector<ConfigVariable<uint32_t> *>::iterator i = Config->canvas_colors.begin(); i != Config->canvas_colors.end(); i++) {
		
		TreeModel::Row row = *(color_list->append());

		Gdk::Color col;
		uint32_t rgba = (*i)->get();
		UINT_TO_RGBA (rgba, &r, &g, &b, &a);
		//cerr << (*i)->name() << " == " << hex << rgba << ": " << hex << r << " " << hex << g << " " << hex << b << endl;
		col.set_rgb_p (r / 255.0, g / 255.0, b / 255.0);

		row[columns.name] = (*i)->name();
		row[columns.color] = "";
		row[columns.pVar] = *i;
		row[columns.rgba] = rgba;
		row[columns.gdkcolor] = col;

		//cerr << (*i)->name() << " == " << rgba << endl;
	}
	cerr << "ThemeManager::setup_theme () called" << endl;
	ColorsChanged(); //EMIT SIGNAL
		
	if (getenv ("ARDOUR2_UI_RC")) {
		return;
	}

	if (Config->get_ui_rc_file() == "ardour2_ui_dark.rc") {
		dark_button.set_active();
	} else if (Config->get_ui_rc_file() == "ardour2_ui_light.rc") {
		light_button.set_active();
	}
	
}

