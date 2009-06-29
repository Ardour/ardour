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
#include <gtkmm/treemodel.h>

#include "ardour/types.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"

#include "ardour_dialog.h"

class Editor;

class AddRouteDialog : public ArdourDialog
{
  public:
	AddRouteDialog (ARDOUR::Session &);
	~AddRouteDialog ();

	bool track ();
	int channels ();
	int count ();

	std::string name_template ();
	std::string track_template ();

	ARDOUR::DataType type();
	ARDOUR::TrackMode mode();
	ARDOUR::RouteGroup* route_group ();

  private:
	ARDOUR::Session& _session;
	Gtk::Entry name_template_entry;
	Gtk::ComboBoxText track_bus_combo;
	Gtk::Adjustment routes_adjustment;
	Gtk::SpinButton routes_spinner;
	Gtk::ComboBoxText channel_combo;
	Gtk::Label track_mode_label;
	Gtk::ComboBoxText track_mode_combo;
	Gtk::ComboBoxText route_group_combo;

	std::vector<ARDOUR::TemplateInfo> route_templates;
	
	void track_type_chosen ();
	void refill_channel_setups ();
	void refill_route_groups ();
	void add_route_group (ARDOUR::RouteGroup *);
	void group_changed ();
	bool channel_separator (const Glib::RefPtr<Gtk::TreeModel> &m, const Gtk::TreeModel::iterator &i);
	bool route_separator (const Glib::RefPtr<Gtk::TreeModel> &m, const Gtk::TreeModel::iterator &i);

	void reset_template_option_visibility ();
	
	void on_show ();

	struct ChannelSetup {
	    std::string name;
	    std::string template_path;
	    uint32_t    channels;
	};

	typedef std::vector<ChannelSetup> ChannelSetups;
	ChannelSetups channel_setups;

	std::vector<std::string> channel_combo_strings;
	std::vector<std::string> track_mode_strings;
};

#endif /* __gtk_ardour_add_route_dialog_h__ */
