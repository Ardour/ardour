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

#ifndef __ardour_gtk_theme_manager_h__
#define __ardour_gtk_theme_manager_h__

#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/button.h>
#include <gtkmm/scale.h>
#include <gtkmm/rc.h>

#include "ui_config.h"

class ArdourDialog;

class ThemeManager : public Gtk::VBox
{
  public:
	ThemeManager();

	void on_flat_buttons_toggled ();
	void on_blink_rec_arm_toggled ();
        void on_region_color_toggled ();
        void on_show_clip_toggled ();
        void on_waveform_gradient_depth_change ();
        void on_timeline_item_gradient_depth_change ();
	void on_all_dialogs_toggled ();
	void on_transients_follow_front_toggled ();
	void on_floating_monitor_section_toggled ();
	void on_icon_set_changed ();

  private:
	Gtk::CheckButton flat_buttons;
	Gtk::CheckButton blink_rec_button;
	Gtk::CheckButton region_color_button;
	Gtk::CheckButton show_clipping_button;
        Gtk::HScale waveform_gradient_depth;
        Gtk::Label waveform_gradient_depth_label;
        Gtk::HScale timeline_item_gradient_depth;
        Gtk::Label timeline_item_gradient_depth_label;
	Gtk::CheckButton all_dialogs;
	Gtk::CheckButton transients_follow_front;
	Gtk::CheckButton floating_monitor_section;
	Gtk::CheckButton gradient_waveforms;
	Gtk::Label icon_set_label;
	Gtk::ComboBoxText icon_set_dropdown;

	void colors_changed ();
	void set_ui_to_state ();
};

#endif /* __ardour_gtk_theme_manager_h__ */
