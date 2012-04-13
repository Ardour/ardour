/*
	Copyright (C) 2010 Paul Davis

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

#include <gtkmm/comboboxtext.h>
#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>

class MackieControlProtocol;

#include "i18n.h"

class MackieControlProtocolGUI : public Gtk::Notebook
{
  public:
    MackieControlProtocolGUI (MackieControlProtocol &);
    
  private:
    void surface_combo_changed ();
    
    MackieControlProtocol& _cp;
    Gtk::ComboBoxText _surface_combo;

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
		add (number);
		add (plain);
		add (shift);
		add (control);
		add (option);
		add (cmdalt);
		add (shiftcontrol);
	};
	Gtk::TreeModelColumn<std::string> name;
	Gtk::TreeModelColumn<uint32_t>    number;
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

    void rebuild_function_key_editor ();
    void action_changed (const Glib::ustring &sPath, const Glib::ustring &text);
};

