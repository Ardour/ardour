/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __gtk2_ardour_startup_h__
#define __gtk2_ardour_startup_h__

#include <string>

#include <gdkmm/pixbuf.h>
#include <gtkmm/assistant.h>
#include <gtkmm/label.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/expander.h>
#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/frame.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/liststore.h>
#include <gtkmm/combobox.h>

#include "ardour/utils.h"

class EngineControl;

class ArdourStartup : public Gtk::Assistant {
  public:
         ArdourStartup ();
	~ArdourStartup ();

        static bool required ();

	gint response () const {
		return  _response;
	}

  private:
	gint _response;
	bool config_modified;
	bool new_user;

	void on_apply ();
	void on_cancel ();
	bool on_delete_event (GdkEventAny*);

	static ArdourStartup *the_startup;

	Glib::RefPtr<Gdk::Pixbuf> icon_pixbuf;

	void setup_new_user_page ();
	Glib::RefPtr<Gdk::Pixbuf> splash_pixbuf;
	Gtk::DrawingArea splash_area;
	bool splash_expose (GdkEventExpose* ev);

	void setup_first_time_config_page ();
        void config_changed ();

	/* first page */
	Gtk::FileChooserButton* default_dir_chooser;
	void default_dir_changed();
	void setup_first_page ();
	Gtk::FileChooserButton new_folder_chooser;

	/* monitoring choices */

	Gtk::VBox mon_vbox;
	Gtk::Label monitor_label;
	Gtk::RadioButton monitor_via_hardware_button;
	Gtk::RadioButton monitor_via_ardour_button;
	void setup_monitoring_choice_page ();

	/* monitor section choices */

	Gtk::VBox mon_sec_vbox;
	Gtk::Label monitor_section_label;
	Gtk::RadioButton use_monitor_section_button;
	Gtk::RadioButton no_monitor_section_button;
	void setup_monitor_section_choice_page ();

	/* final page */

	void setup_final_page ();
	Gtk::Label final_page;

	/* always there */

	Glib::RefPtr<Pango::Layout> layout;

	/* page indices */

	gint audio_page_index;
	gint new_user_page_index;
	gint default_folder_page_index;
	gint monitoring_page_index;
	gint monitor_section_page_index;
	gint final_page_index;

	void move_along_now ();
};

#endif /* __gtk2_ardour_startup_h__ */
