/*
 * Copyright (C) 2026 Holger Dehnhardt <holger@dehnhardt.org>
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

#ifndef __ardour_console1_gui_h__
#define __ardour_console1_gui_h__

#include <vector>
#include <string>   

#include <ytkmm/box.h>
#include <ytkmm/button.h>
#include <ytkmm/combobox.h>
#include <ytkmm/checkbutton.h>
#include <ytkmm/image.h>
#include <ytkmm/table.h>
#include <ytkmm/treestore.h>
#include <ytkmm/spinbutton.h>
#include <ytkmm/notebook.h>
#include <ytkmm/scrolledwindow.h>
#include <ytkmm/togglebutton.h>
#include <ytkmm/treeview.h>

namespace Gtk {
	class CellRendererCombo;
	class ListStore;
}

#include "c1_panel.h"
#include "console1.h"

namespace ArdourSurface {

class C1GUI : public Gtk::VBox
{
public:
	C1GUI (Console1&);
	~C1GUI ();

private:
	Console1& c1;
	PBD::ScopedConnectionList lcxl_connections;
	Gtk::VBox hpacker;
	Gtk::Table table;
	Gtk::ComboBox input_combo;
	Gtk::ComboBox output_combo;
	C1Panel* panel;
	Gtk::CheckButton edit_labels_cb;
	Gtk::Button reset_labels_btn;
	Gtk::HBox label_edit_box;
	void set_edit_labels ();
	Gtk::CheckButton swap_solo_mute_cb;
	Gtk::CheckButton band_q_as_send_cb;
	Gtk::CheckButton create_plugin_stubs_btn;

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

	void set_swap_solo_mute ();
	void set_band_q_as_send();
	void set_create_mapping_stubs();

	/* plugin assignment page (c1_plugin_control_assignment.cc) */
	struct PluginColumns : public Gtk::TreeModel::ColumnRecord {
		PluginColumns () {
			add (plugin_name);
			add (plugin_id);
		}
		Gtk::TreeModelColumn<std::string> plugin_name;
		Gtk::TreeModelColumn<std::string> plugin_id;
	};

	struct PluginAssignmentEditorColumns : public Gtk::TreeModel::ColumnRecord {
		PluginAssignmentEditorColumns () {
			add (index);
			add (name);
			add (is_switch);
			add (controllerName);
			add (shift);
			add (learn);
		}
		Gtk::TreeModelColumn<int> index;
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<bool> is_switch;
		Gtk::TreeModelColumn<std::string> controllerName;
		Gtk::TreeModelColumn<bool> shift;
		Gtk::TreeModelColumn<bool> learn;
	};

	Gtk::Notebook notebook;
	Gtk::ScrolledWindow plugin_mapping_scroller;
	Gtk::ComboBox plugins_combo;
	Gtk::TreeView plugin_assignment_editor;
	sigc::signal<void> plugin_assignment_changed;
	sigc::connection assign_connection;
	PBD::ScopedConnectionList plugin_connections;
	PluginColumns plugin_columns;
	PluginAssignmentEditorColumns plugin_assignment_editor_columns;
	Glib::RefPtr<Gtk::ListStore> plugin_assignment_store;
	Console1::PluginMapping plugin_mapping;

	Gtk::VBox* build_plugin_assignment_page ();
	Gtk::CellRendererCombo* make_action_renderer (Glib::RefPtr<Gtk::ListStore> model, Gtk::TreeModelColumnBase column);
	void load_plugin_combo_rows ();
	void build_plugin_assignment_editor ();
	void change_controller_number (int controllerNumber, bool shiftState);
	void toggle_learn (const Glib::ustring&);
	void stop_midi_assign ();
	void change_controller (const Glib::ustring&, const Gtk::TreeIter&);
	void toggle_shift (const Glib::ustring&);
	void active_plugin_changed (Gtk::ComboBox* combo);
	void write_plugin_assignment ();
};

}

#endif /* __ardour_console1_gui_h__ */
