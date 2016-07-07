/*
    Copyright (C) 2015 Paul Davis

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
	class CellRendererCombo;
	class ListStore;
}

#include "push2.h"
#include "mode.h"

namespace ArdourSurface {

class P2GUI : public Gtk::VBox
{
public:
	P2GUI (Push2&);
	~P2GUI ();

	void build_pad_table ();

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
	PBD::ScopedConnection connection_change_connection;
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

	void build_available_action_menu ();
	bool find_action_in_model (const Gtk::TreeModel::iterator& iter, std::string const & action_path, Gtk::TreeModel::iterator* found);

	/* Pads */

	Gtk::Table pad_table;

	/* root notes */

	Gtk::Adjustment root_note_octave_adjustment;
	Gtk::SpinButton root_note_octave;
	Gtk::Label root_note_octave_label;

	void root_note_octave_adjustment_changed ();

	struct NoteColumns : public Gtk::TreeModel::ColumnRecord {
		NoteColumns () {
			add (number);
			add (name);
		}
		Gtk::TreeModelColumn<int>         number;
		Gtk::TreeModelColumn<std::string> name;
	};
	NoteColumns note_columns;
	Glib::RefPtr<Gtk::ListStore> build_note_columns ();
	Gtk::ComboBox root_note_selector;
	Gtk::Label root_note_label;

	void root_note_changed ();

	/* modes/scales */

	struct ModeColumns : public Gtk::TreeModel::ColumnRecord {
		ModeColumns () {
			add (mode);
			add (name);
		}
		Gtk::TreeModelColumn<MusicalMode::Type>  mode;
		Gtk::TreeModelColumn<std::string> name;
	};
	ModeColumns mode_columns;
	Glib::RefPtr<Gtk::ListStore> build_mode_columns ();
	Gtk::ComboBox mode_selector;
	Gtk::Label mode_label;

	Gtk::CheckButton inkey_button;

	Gtk::Notebook pad_notebook;
	Gtk::Table    mode_packer;
	Gtk::VBox     custom_packer;

	void reprogram_pad_scale ();
};

}

#endif /* __ardour_push2_gui_h__ */
