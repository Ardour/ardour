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

#ifndef __ardour_gtk_color_manager_h__
#define __ardour_gtk_color_manager_h__

#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/colorselection.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/button.h>
#include <gtkmm/scale.h>
#include <gtkmm/rc.h>
#include "ardour_window.h"
#include "ui_config.h"

class ThemeManager : public ArdourWindow
{
  public:
	ThemeManager();
	~ThemeManager();

	int save (std::string path);
	void setup_theme ();
	void reset_canvas_colors();

	void on_dark_theme_button_toggled ();
	void on_light_theme_button_toggled ();
	void on_flat_buttons_toggled ();
        void on_waveform_gradient_depth_change ();

  private:
	struct ColorDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    ColorDisplayModelColumns() {
		    add (name);
		    add (gdkcolor);
		    add (pVar);
		    add (rgba);
	    }

	    Gtk::TreeModelColumn<std::string>  name;
	    Gtk::TreeModelColumn<Gdk::Color>   gdkcolor;
	    Gtk::TreeModelColumn<UIConfigVariable<uint32_t> *> pVar;
	    Gtk::TreeModelColumn<uint32_t>     rgba;
	};

	ColorDisplayModelColumns columns;
	Gtk::TreeView color_display;
	Glib::RefPtr<Gtk::TreeStore> color_list;
	Gtk::ColorSelectionDialog color_dialog;
	Gtk::ScrolledWindow scroller;
	Gtk::HBox theme_selection_hbox;
	Gtk::RadioButton dark_button;
	Gtk::RadioButton light_button;
	Gtk::Button reset_button;
	Gtk::CheckButton flat_buttons;
        Gtk::HScale waveform_gradient_depth;
        Gtk::Label waveform_gradient_depth_label;

	bool button_press_event (GdkEventButton*);
};

#endif /* __ardour_gtk_color_manager_h__ */

