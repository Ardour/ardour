/*
    Copyright (C) 2000-2009 Paul Davis

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

#include "editor_component.h"

class EditorRouteGroups : public EditorComponent, public ARDOUR::SessionHandlePtr
{
public:
	EditorRouteGroups (Editor *);

	void set_session (ARDOUR::Session *);

	Gtk::Widget& widget () {
		return _display_packer;
	}

	void clear ();
        Gtk::ToggleButton& all_group_active_button() { return _all_group_active_button; }

private:

        struct Columns : public Gtk::TreeModel::ColumnRecord {

                Columns () {
			add (is_visible);
			add (gain);
			add (record);
			add (mute);
			add (solo);
			add (select);
			add (edits);
			add (text);
			add (routegroup);
                }

	        Gtk::TreeModelColumn<bool> is_visible;
		Gtk::TreeModelColumn<bool> gain;
		Gtk::TreeModelColumn<bool> record;
		Gtk::TreeModelColumn<bool> mute;
		Gtk::TreeModelColumn<bool> solo;
		Gtk::TreeModelColumn<bool> select;
		Gtk::TreeModelColumn<bool> edits;
	        Gtk::TreeModelColumn<std::string> text;
	        Gtk::TreeModelColumn<ARDOUR::RouteGroup*> routegroup;
	};

	Columns _columns;

	void add (ARDOUR::RouteGroup *);
	void row_change (const Gtk::TreeModel::Path&,const Gtk::TreeModel::iterator&);
	void name_edit (const Glib::ustring&, const Glib::ustring&);
	void button_clicked ();
	gint button_press_event (GdkEventButton* ev);
	void groups_changed ();
	void property_changed (ARDOUR::RouteGroup*, const PBD::PropertyChange &);
	void remove_selected ();
	void run_new_group_dialog ();
        void all_group_toggled();
        void all_group_changed (const PBD::PropertyChange&);

	Glib::RefPtr<Gtk::ListStore> _model;
	Glib::RefPtr<Gtk::TreeSelection> _selection;
	Gtk::TreeView _display;
	Gtk::ScrolledWindow _scroller;
	Gtk::VBox _display_packer;
        Gtk::ToggleButton _all_group_active_button;
	bool _in_row_change;
	PBD::ScopedConnection property_changed_connection;
};


