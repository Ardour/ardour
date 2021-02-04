/*
 * Copyright (C) 2016 W.P. van Paassen
 * Copyright (C) 2016-2019 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_cc121_gui_h__
#define __ardour_cc121_gui_h__

#include <vector>
#include <string>

#include <gtkmm/box.h>
#include <gtkmm/combobox.h>
#include <gtkmm/image.h>
#include <gtkmm/table.h>
#include <gtkmm/treestore.h>

namespace Gtk {
	class CellRendererCombo;
	class ListStore;
}

#include "cc121.h"

namespace ActionManager {
        class ActionModel;
}

namespace ArdourSurface {

class CC121GUI : public Gtk::VBox
{
public:
	CC121GUI (CC121&);
	~CC121GUI ();

private:
	CC121& fp;
	Gtk::HBox hpacker;
	Gtk::Table table;
	Gtk::Table action_table;
	Gtk::ComboBox input_combo;
	Gtk::ComboBox output_combo;
	Gtk::Image    image;

	Gtk::ComboBox foot_combo;
	Gtk::ComboBox function1_combo;
	Gtk::ComboBox function2_combo;
	Gtk::ComboBox function3_combo;
	Gtk::ComboBox function4_combo;
	Gtk::ComboBox value_combo;
	Gtk::ComboBox lock_combo;
	Gtk::ComboBox eq1_combo;
	Gtk::ComboBox eq2_combo;
	Gtk::ComboBox eq3_combo;
	Gtk::ComboBox eq4_combo;
	Gtk::ComboBox eqtype_combo;
	Gtk::ComboBox allbypass_combo;

	void update_port_combos ();
	PBD::ScopedConnectionList _port_connections;
	void connection_handler ();

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

	const ActionManager::ActionModel& action_model;

	Glib::RefPtr<Gtk::TreeStore> available_action_model;
	std::map<std::string,std::string> action_map; // map from action names to paths

	void build_action_combo (Gtk::ComboBox& cb, std::vector<std::pair<std::string,std::string> > const & actions, CC121::ButtonID, CC121::ButtonState);
	void build_user_action_combo (Gtk::ComboBox&, CC121::ButtonState, CC121::ButtonID);
	void build_foot_action_combo (Gtk::ComboBox&, CC121::ButtonState);

	void action_changed (Gtk::ComboBox*, CC121::ButtonID, CC121::ButtonState);
};

}

#endif /* __ardour_cc121_gui_h__ */
