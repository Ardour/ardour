/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Paul Davis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __ardour_faderport8_gui_h__
#define __ardour_faderport8_gui_h__

#include <vector>
#include <string>

#include <gtkmm/box.h>
#include <gtkmm/combobox.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/image.h>
#include <gtkmm/table.h>
#include <gtkmm/treestore.h>

namespace Gtk {
	class CellRendererCombo;
	class ListStore;
}

#include "faderport8.h"

namespace ArdourSurface { namespace FP_NAMESPACE {

class FP8GUI : public Gtk::VBox
{
public:
	FP8GUI (FaderPort8&);
	~FP8GUI ();

private:
	FaderPort8& fp;
	Gtk::HBox hpacker;
	Gtk::Table table;
	Gtk::Image image;

	/* port connections */
	Gtk::ComboBox input_combo;
	Gtk::ComboBox output_combo;

	void update_port_combos ();
	void connection_handler ();
	PBD::ScopedConnection connection_change_connection;

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

	/* misc Prefs */
	Gtk::ComboBoxText clock_combo;
	Gtk::ComboBoxText scribble_combo;
	Gtk::CheckButton  two_line_text_cb;
	Gtk::CheckButton  auto_pluginui_cb;

	void build_prefs_combos ();
	void update_prefs_combos ();
	void clock_mode_changed ();
	void scribble_mode_changed ();
	void twolinetext_toggled ();
	void auto_pluginui_toggled ();

	/* user actions */
	void build_available_action_menu ();
	void build_action_combo (Gtk::ComboBox& cb, FP8Controls::ButtonId id);
	void action_changed (Gtk::ComboBox* cb, FP8Controls::ButtonId id);

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

	bool find_action_in_model (const Gtk::TreeModel::iterator& iter, std::string const & action_path, Gtk::TreeModel::iterator* found);
};

} }

#endif /* __ardour_faderport8_gui_h__ */
