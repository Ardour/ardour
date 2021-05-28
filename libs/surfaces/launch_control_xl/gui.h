/*
 * Copyright (C) 2018 Jan Lentfer <jan.lentfer@web.de>
 * Copyright (C) 2018 Térence Clastres <t.clastres@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_launch_control_gui_h__
#define __ardour_launch_control_gui_h__

#include <vector>
#include <string>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/combobox.h>
#include <gtkmm/image.h>
#include <gtkmm/table.h>
#include <gtkmm/treestore.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/notebook.h>

namespace Gtk {
	class CellRendererCombo;
	class ListStore;
}

#include "ardour/mode.h"

#include "launch_control_xl.h"

namespace ArdourSurface {

class LCXLGUI : public Gtk::VBox
{
public:
	LCXLGUI (LaunchControlXL&);
	~LCXLGUI ();

	void toggle_fader8master ();
	void toggle_ctrllowersends ();

private:
	LaunchControlXL& lcxl;
	PBD::ScopedConnectionList lcxl_connections;
	Gtk::HBox hpacker;
	Gtk::Table table;
	Gtk::Table action_table;
	Gtk::ComboBox input_combo;
	Gtk::ComboBox output_combo;
	Gtk::Image    image;
	Gtk::CheckButton fader8master_button;
	Gtk::CheckButton ctrllowersends_button;

	void update_port_combos ();
	PBD::ScopedConnection connection_change_connection;
	void connection_handler ();
	PBD::ScopedConnectionList _port_connections;

	struct MidiPortColumns : public Gtk::TreeModel::ColumnRecord {
		MidiPortColumns() {
			add (short_name);
			add (full_name);
		}
		Gtk::TreeModelColumn<std::string> short_name;
		Gtk::TreeModelColumn<std::string> full_name;
	};

	MidiPortColumns midi_port_columns;
	bool ignore_active_change;

	Glib::RefPtr<Gtk::ListStore> build_midi_port_list (std::vector<std::string> const & ports, bool for_input);
	void active_port_changed (Gtk::ComboBox*,bool for_input);

	struct ActionColumns : public Gtk::TreeModel::ColumnRecord {
		ActionColumns() {
			add (name);
			add (path);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
	};

	ActionColumns action_columns;
	Glib::RefPtr<Gtk::TreeStore> available_action_model;
	std::map<std::string,std::string> action_map; // map from action names to paths


};

}

#endif /* __ardour_launch_control_gui_h__ */
