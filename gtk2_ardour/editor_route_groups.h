/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_editor_route_groups_h__
#define __gtk_ardour_editor_route_groups_h__

#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>

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

private:

	struct Columns : public Gtk::TreeModel::ColumnRecord {

		Columns () {
			add (gdkcolor);
			add (text);
			add (is_visible);
			add (gain);
			add (gain_relative);
			add (mute);
			add (solo);
			add (record);
			add (monitoring);
			add (select);
			add (active_shared);
			add (active_state);
			add (routegroup);
		}

		Gtk::TreeModelColumn<Gdk::Color> gdkcolor;
		Gtk::TreeModelColumn<std::string> text;
		Gtk::TreeModelColumn<bool> is_visible;
		Gtk::TreeModelColumn<bool> gain;
		Gtk::TreeModelColumn<bool> gain_relative;
		Gtk::TreeModelColumn<bool> mute;
		Gtk::TreeModelColumn<bool> solo;
		Gtk::TreeModelColumn<bool> record;
		Gtk::TreeModelColumn<bool> monitoring;
		Gtk::TreeModelColumn<bool> select;
		Gtk::TreeModelColumn<bool> active_shared;
		Gtk::TreeModelColumn<bool> active_state;
		Gtk::TreeModelColumn<ARDOUR::RouteGroup*> routegroup;
	};

	Columns _columns;

	void add (ARDOUR::RouteGroup *);
	void row_change (const Gtk::TreeModel::Path&,const Gtk::TreeModel::iterator&);
	void name_edit (const std::string&, const std::string&);
	void button_clicked ();
	bool button_press_event (GdkEventButton* ev);
	void groups_changed ();
	void property_changed (ARDOUR::RouteGroup*, const PBD::PropertyChange &);
	void remove_selected ();
	void run_new_group_dialog ();
	void row_deleted (Gtk::TreeModel::Path const &);

	Glib::RefPtr<Gtk::ListStore> _model;
	Glib::RefPtr<Gtk::TreeSelection> _selection;
	Gtk::TreeView _display;
	Gtk::ScrolledWindow _scroller;
	Gtk::VBox _display_packer;
	bool _in_row_change;
	bool _in_rebuild;
	PBD::ScopedConnectionList _property_changed_connections;
	PBD::ScopedConnection all_route_groups_changed_connection;
	Gtk::ColorSelectionDialog color_dialog;
};

#endif // __gtk_ardour_editor_route_groups_h__
