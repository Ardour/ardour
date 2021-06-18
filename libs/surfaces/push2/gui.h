/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_push2_gui_h__
#define __ardour_push2_gui_h__

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
	class ListStore;
}

#include "ardour/mode.h"

#include "push2.h"

namespace ArdourSurface {

class P2GUI : public Gtk::VBox
{
public:
	P2GUI (Push2&);
	~P2GUI ();

private:
	Push2& p2;
	PBD::ScopedConnectionList p2_connections;
	Gtk::HBox hpacker;
	Gtk::Table table;
	Gtk::Table action_table;
	Gtk::ComboBox input_combo;
	Gtk::ComboBox output_combo;
	Gtk::Image    image;

	void update_port_combos ();
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

	struct PressureModeColumns : public Gtk::TreeModel::ColumnRecord {
		PressureModeColumns() {
			add (mode);
			add (name);
		}
		Gtk::TreeModelColumn<Push2::PressureMode>  mode;
		Gtk::TreeModelColumn<std::string> name;
	};
	PressureModeColumns pressure_mode_columns;
	Glib::RefPtr<Gtk::ListStore> build_pressure_mode_columns ();
	Gtk::ComboBox pressure_mode_selector;
	Gtk::Label pressure_mode_label;

	void reprogram_pressure_mode ();
};

}

#endif /* __ardour_push2_gui_h__ */
