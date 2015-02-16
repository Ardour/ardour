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
#include <map>

#include <gtkmm/buttonbox.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>
#include "waves_dialog.h"

class KeyEditor : public WavesDialog
{
  public:
	KeyEditor ();

  protected:
	void on_show ();
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

	Glib::RefPtr<Gtk::TreeStore> model;
	KeyEditorColumns columns;
	Gtk::TreeView& view;
	WavesButton& unbind_button;
	WavesButton& reset_button;

	static const char* blacklist_filename;
	static const char* whitelist_filename;

	void unbind (WavesButton*);

	bool can_bind;
	guint last_state;

	void action_selected ();
	void populate ();

	void reset (WavesButton*);

    std::map<std::string,std::string> action_blacklist;
    std::map<std::string,std::string> action_whitelist;
    void load_blackwhitelists ();
};

#endif /* __ardour_gtk_key_editor_h__ */
