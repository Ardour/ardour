/*
 * Copyright (C) 2015-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2019 Johannes Mueller <github@johannes-mueller.org>
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

#ifndef __ardour_faderport_gui_h__
#define __ardour_faderport_gui_h__

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

#include "faderport.h"

namespace ActionManager {
        class ActionModel;
}

namespace ArdourSurface {

class FPGUI : public Gtk::VBox
{
public:
	FPGUI (FaderPort&);
	~FPGUI ();

private:
	FaderPort& fp;
	Gtk::HBox hpacker;
	Gtk::Table table;
	Gtk::Table action_table;
	Gtk::ComboBox input_combo;
	Gtk::ComboBox output_combo;
	Gtk::Image    image;

	/* the mix, proj, trns and user buttons have no obvious semantics for
	 * ardour, mixbus etc., so we allow the user to define their
	 * functionality from a small, curated set of options.
	 */

	Gtk::ComboBox mix_combo[3];
	Gtk::ComboBox proj_combo[3];
	Gtk::ComboBox trns_combo[3];
	Gtk::ComboBox user_combo[2];
	Gtk::ComboBox foot_combo[3];

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

	std::map<std::string,std::string> action_map; // map from action names to paths

	void build_action_combo (Gtk::ComboBox& cb, std::vector<std::pair<std::string,std::string> > const & actions, FaderPort::ButtonID, FaderPort::ButtonState);
	void build_mix_action_combo (Gtk::ComboBox&, FaderPort::ButtonState);
	void build_proj_action_combo (Gtk::ComboBox&, FaderPort::ButtonState);
	void build_trns_action_combo (Gtk::ComboBox&, FaderPort::ButtonState);
	void build_user_action_combo (Gtk::ComboBox&, FaderPort::ButtonState);
	void build_foot_action_combo (Gtk::ComboBox&, FaderPort::ButtonState);

	void action_changed (Gtk::ComboBox*, FaderPort::ButtonID, FaderPort::ButtonState);
};

}

#endif /* __ardour_faderport_gui_h__ */
