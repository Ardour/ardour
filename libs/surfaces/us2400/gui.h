/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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

#include <vector>

#include <gtkmm/combobox.h>
#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>

namespace Gtk {
	class CellRendererCombo;
}

#include "button.h"

#include "pbd/i18n.h"

namespace ActionManager {
        class ActionModel;
}

namespace ArdourSurface {

class US2400Protocol;

namespace US2400 {
	class Surface;
}

class US2400ProtocolGUI : public Gtk::Notebook
{
   public:
	US2400ProtocolGUI (US2400Protocol &);

   private:
	US2400Protocol& _cp;
	Gtk::Table         table;
	Gtk::ComboBoxText _profile_combo;

	typedef std::vector<Gtk::ComboBox*> PortCombos;
	PortCombos input_combos;
	PortCombos output_combos;

	struct MidiPortColumns : public Gtk::TreeModel::ColumnRecord {
		MidiPortColumns() {
			add (short_name);
			add (full_name);
		}
		Gtk::TreeModelColumn<std::string> short_name;
		Gtk::TreeModelColumn<std::string> full_name;
	};

	struct FunctionKeyColumns : public Gtk::TreeModel::ColumnRecord {
		FunctionKeyColumns() {
			add (name);
			add (id);
			add (plain);
			add (shift);
			add (control);
			add (option);
			add (cmdalt);
			add (shiftcontrol);
		};
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<US2400::Button::ID>  id;
		Gtk::TreeModelColumn<std::string> plain;
		Gtk::TreeModelColumn<std::string> shift;
		Gtk::TreeModelColumn<std::string> control;
		Gtk::TreeModelColumn<std::string> option;
		Gtk::TreeModelColumn<std::string> cmdalt;
		Gtk::TreeModelColumn<std::string> shiftcontrol;
	};

	FunctionKeyColumns function_key_columns;
	MidiPortColumns midi_port_columns;

	Gtk::ScrolledWindow function_key_scroller;
	Gtk::TreeView function_key_editor;
	Glib::RefPtr<Gtk::ListStore> function_key_model;
	Glib::RefPtr<Gtk::TreeStore> available_action_model;

	Glib::RefPtr<Gtk::ListStore> build_midi_port_list (bool for_input);

	const ActionManager::ActionModel& action_model;

	void refresh_function_key_editor ();
	void build_function_key_editor ();
	void action_changed (const Glib::ustring &sPath, const Gtk::TreeModel::iterator &, Gtk::TreeModelColumnBase);
	Gtk::CellRendererCombo* make_action_renderer (Glib::RefPtr<Gtk::TreeStore> model, Gtk::TreeModelColumnBase);

	void profile_combo_changed ();

	Gtk::Widget* device_dependent_widget ();
	Gtk::Widget* _device_dependent_widget;
	int device_dependent_row;

	PBD::ScopedConnection device_change_connection;
	void device_changed ();

	void update_port_combos (std::vector<std::string> const&, std::vector<std::string> const&,
	                         Gtk::ComboBox* input_combo,
	                         Gtk::ComboBox* output_combo,
	                         boost::shared_ptr<US2400::Surface> surface);

	PBD::ScopedConnectionList _port_connections;
	void connection_handler ();

	Glib::RefPtr<Gtk::ListStore> build_midi_port_list (std::vector<std::string> const & ports, bool for_input);
	bool _ignore_profile_changed;
	bool ignore_active_change;
	void active_port_changed (Gtk::ComboBox* combo, boost::weak_ptr<US2400::Surface> ws, bool for_input);
};

}
