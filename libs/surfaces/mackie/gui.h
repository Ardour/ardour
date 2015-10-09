/*
  Copyright (C) 2010-2012 Paul Davis

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <vector>

#include <gtkmm/comboboxtext.h>
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

#include "i18n.h"

namespace ArdourSurface {

class MackieControlProtocol;

namespace Mackie {
	class Surface;
}

class MackieControlProtocolGUI : public Gtk::Notebook
{
   public:
	MackieControlProtocolGUI (MackieControlProtocol &);

   private:
	MackieControlProtocol& _cp;
	Gtk::Table         table;
	Gtk::ComboBoxText _surface_combo;
	Gtk::ComboBoxText _profile_combo;

	typedef std::vector<Gtk::ComboBoxText*> PortCombos;
	PortCombos input_combos;
	PortCombos output_combos;

	struct AvailableActionColumns : public Gtk::TreeModel::ColumnRecord {
		AvailableActionColumns() {
			add (name);
			add (path);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
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
		Gtk::TreeModelColumn<Mackie::Button::ID>  id;
		Gtk::TreeModelColumn<std::string> plain;
		Gtk::TreeModelColumn<std::string> shift;
		Gtk::TreeModelColumn<std::string> control;
		Gtk::TreeModelColumn<std::string> option;
		Gtk::TreeModelColumn<std::string> cmdalt;
		Gtk::TreeModelColumn<std::string> shiftcontrol;
	};

	AvailableActionColumns available_action_columns;
	FunctionKeyColumns function_key_columns;

	Gtk::ScrolledWindow function_key_scroller;
	Gtk::TreeView function_key_editor;
	Glib::RefPtr<Gtk::ListStore> function_key_model;
	Glib::RefPtr<Gtk::TreeStore> available_action_model;

	void build_available_action_menu ();
	void refresh_function_key_editor ();
	void build_function_key_editor ();
	void action_changed (const Glib::ustring &sPath, const Glib::ustring &text, Gtk::TreeModelColumnBase);
	Gtk::CellRendererCombo* make_action_renderer (Glib::RefPtr<Gtk::TreeStore> model, Gtk::TreeModelColumnBase);

	void surface_combo_changed ();
	void profile_combo_changed ();
	void ipmidi_spinner_changed ();

	std::map<std::string,std::string> action_map; // map from action names to paths

	Gtk::CheckButton relay_click_button;
	Gtk::CheckButton backlight_button;
	Gtk::RadioButton absolute_touch_mode_button;
	Gtk::RadioButton touch_move_mode_button;
	Gtk::Adjustment  touch_sensitivity_adjustment;
	Gtk::HScale      touch_sensitivity_scale;
	Gtk::Button      recalibrate_fader_button;
	Gtk::Adjustment  ipmidi_base_port_adjustment;
	Gtk::Button      discover_button;

	void discover_clicked ();
	void recalibrate_faders ();
	void toggle_backlight ();
	void touch_sensitive_change ();

	Gtk::Widget* device_dependent_widget ();
	Gtk::Widget* _device_dependent_widget;
	int device_dependent_row;

	PBD::ScopedConnection device_change_connection;
	void device_changed ();

	void update_port_combos (std::vector<std::string> const&, std::vector<std::string> const&,
	                         Gtk::ComboBoxText* input_combo,
	                         Gtk::ComboBoxText* output_combo,
	                         boost::shared_ptr<Mackie::Surface> surface);

	/* this takes a raw pointer to Surface, because it connects to a signal
	   emitted by a Surface and we don't want to use
	   boost::shared_from_this.
	*/
	void surface_connectivity_change (Mackie::Surface* raw_surface);
};

}

