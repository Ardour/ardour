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

#ifndef __gtk_ardour_add_route_dialog_h__
#define __gtk_ardour_add_route_dialog_h__

#include <string>

#include <gtkmm/entry.h>
#include <gtkmm/dialog.h>
#include <gtkmm/frame.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>

#include "ardour/types.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"

class AddRouteDialog : public Gtk::Dialog
{
  public:
	AddRouteDialog ();
	~AddRouteDialog ();

	bool track ();
	int channels ();
	int count ();

	std::string name_template ();
	std::string track_template ();

	ARDOUR::DataType type();
	ARDOUR::TrackMode mode();

  private:
	Gtk::Entry name_template_entry;
	Gtk::RadioButton track_button;
	Gtk::RadioButton bus_button;
	Gtk::RadioButton template_button;
	Gtk::Adjustment routes_adjustment;
	Gtk::SpinButton routes_spinner;
	Gtk::ComboBoxText channel_combo;
	Gtk::ComboBoxText track_mode_combo;
	Gtk::ComboBoxText track_template_combo;

	std::vector<ARDOUR::RouteTemplateInfo> route_templates;
	
	void track_type_chosen ();
	void refill_track_templates ();

	Gtk::HBox* hbox3;
	Gtk::HBox* hbox9;
	Gtk::HBox* hbox4;
	
	void reset_template_option_visibility ();
	
	void on_show ();
};

#endif /* __gtk_ardour_add_route_dialog_h__ */
