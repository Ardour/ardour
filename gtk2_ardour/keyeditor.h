/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __ardour_gtk_key_editor_h__
#define __ardour_gtk_key_editor_h__

#include <string>

#include <gtkmm/buttonbox.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour_window.h"

class KeyEditor : public ArdourWindow
{
  public:
	KeyEditor ();

  protected:
	void on_show ();
	void on_unmap ();
	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

  private:
	struct KeyEditorColumns : public Gtk::TreeModel::ColumnRecord {
	    KeyEditorColumns () {
		    add (action);
		    add (binding);
		    add (path);
		    add (bindable);
	    }
	    Gtk::TreeModelColumn<std::string> action;
	    Gtk::TreeModelColumn<std::string> binding;
	    Gtk::TreeModelColumn<std::string> path;
	    Gtk::TreeModelColumn<bool> bindable;
	};

        Gtk::VBox vpacker;
	Gtk::ScrolledWindow scroller;
	Gtk::TreeView view;
	Glib::RefPtr<Gtk::TreeStore> model;
	KeyEditorColumns columns;
	Gtk::Button unbind_button;
	Gtk::HButtonBox unbind_box;

	void unbind ();

	bool can_bind;
	guint last_state;

	void action_selected ();
	void populate ();
};

#endif /* __ardour_gtk_key_editor_h__ */
