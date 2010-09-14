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

#include <gtkmm/widget.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>
#include "editor_component.h"

class EditorSnapshots : public EditorComponent, public ARDOUR::SessionHandlePtr
{
public:
	EditorSnapshots (Editor *);

	void set_session (ARDOUR::Session *);

	Gtk::Widget& widget () {
		return _scroller;
	}

	void redisplay ();

private:

	Gtk::ScrolledWindow _scroller;

	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns () {
			add (visible_name);
			add (real_name);
		}
		Gtk::TreeModelColumn<std::string> visible_name;
		Gtk::TreeModelColumn<std::string> real_name;
	};

	Columns _columns;
	Glib::RefPtr<Gtk::ListStore> _model;
	Gtk::TreeView _display;
	Gtk::Menu _menu;

	bool button_press (GdkEventButton *);
	void selection_changed ();
	void popup_context_menu (int, int32_t, std::string);
	void remove (std::string);
	void rename (std::string);
};
