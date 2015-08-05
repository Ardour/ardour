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
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>

#include "ardour_window.h"

namespace Gtkmm2ext {
	class Bindings;
}

class KeyEditor : public ArdourWindow
{
  public:
	KeyEditor ();

	void add_tab (std::string const &name, Gtkmm2ext::Bindings&);
	
  protected:
	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

  private:
	class Tab : public Gtk::VBox
	{
 	   public:
		Tab (KeyEditor&, std::string const &name, Gtkmm2ext::Bindings*);
		
		void populate ();
		void unbind ();
		void bind (GdkEventKey* release_event, guint pressed_key);
		void action_selected ();
		
		struct KeyEditorColumns : public Gtk::TreeModel::ColumnRecord {
			KeyEditorColumns () {
				add (name);
				add (binding);
				add (path);
				add (bindable);
				add (action);
			}
			Gtk::TreeModelColumn<std::string> name;
			Gtk::TreeModelColumn<std::string> binding;
			Gtk::TreeModelColumn<std::string> path;
			Gtk::TreeModelColumn<bool> bindable;
			Gtk::TreeModelColumn<Glib::RefPtr<Gtk::Action> > action;
		};
		
		Gtk::VBox vpacker;
		/* give KeyEditor full access to these. This is just a helper
		   class with no special semantics
		*/
		
		KeyEditor& owner;
		std::string name;
		Gtkmm2ext::Bindings* bindings;
		Gtk::ScrolledWindow scroller;
		Gtk::TreeView view;
		Glib::RefPtr<Gtk::TreeStore> model;
		KeyEditorColumns columns;
	};

	friend class Tab;
	
	Gtk::VBox vpacker;
	Gtk::Notebook notebook;
	Gtk::Button unbind_button;
	Gtk::HButtonBox unbind_box;
	Gtk::HBox reset_box;
	Gtk::Button reset_button;
	Gtk::Label reset_label;
	guint last_keyval;

	typedef std::vector<Tab*> Tabs;

	Tabs tabs;
	Tab* current_tab();

	void unbind ();
	void reset ();
	void page_change (GtkNotebookPage*, guint);
};

#endif /* __ardour_gtk_key_editor_h__ */
