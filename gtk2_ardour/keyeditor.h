/*
 * Copyright (C) 2007-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_key_editor_h__
#define __ardour_gtk_key_editor_h__

#include <string>

#include <gtkmm/buttonbox.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodelfilter.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>

#include "widgets/searchbar.h"

#include "ardour_window.h"

namespace Gtkmm2ext {
	class Bindings;
}

class KeyEditor : public ArdourWindow
{
public:
	KeyEditor ();

	void add_tab (std::string const &name, Gtkmm2ext::Bindings&);
	void remove_tab (std::string const &name);

	static sigc::signal<void> UpdateBindings;

	void disconnect () {
		_refresh_connection.disconnect ();
	}

private:
	class Tab : public Gtk::VBox
	{
	public:
		Tab (KeyEditor&, std::string const &name, Gtkmm2ext::Bindings*);

		uint32_t populate ();
		void unbind ();
		void bind (GdkEventKey* release_event, guint pressed_key);
		void action_selected ();
		void sort_column_changed ();
		void tab_mapped ();
		bool visible_func(const Gtk::TreeModel::const_iterator& iter) const;

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
		 * class with no special semantics
		 */

		KeyEditor& owner;
		std::string name;
		Gtkmm2ext::Bindings* bindings;
		Gtk::ScrolledWindow scroller;
		Gtk::TreeView view;
		Glib::RefPtr<Gtk::TreeStore> data_model;
		Glib::RefPtr<Gtk::TreeModelFilter> filter;
		Glib::RefPtr<Gtk::TreeModelSort> sorted_filter;
		KeyEditorColumns columns;
		guint last_keyval;

	protected:
		bool key_press_event (GdkEventKey*);
		bool key_release_event (GdkEventKey*);
		Gtk::TreeModel::iterator find_action_path (Gtk::TreeModel::const_iterator begin, Gtk::TreeModel::const_iterator end, const std::string& path) const;
	};

	friend class Tab;

	Gtk::VBox vpacker;
	Gtk::Notebook notebook;
	Gtk::Button unbind_button;
	Gtk::HButtonBox unbind_box;
	Gtk::HBox reset_box;
	Gtk::Button reset_button;
	Gtk::Label reset_label;
	ArdourWidgets::SearchBar filter_entry;
	std::string filter_string;
	Gtk::Button print_button;
	Gtk::Label print_label;

	typedef std::vector<Tab*> Tabs;

	Tabs tabs;
	Tab* current_tab();

	void unbind ();
	void reset ();
	void refresh ();
	void page_change (GtkNotebookPage*, guint);

	unsigned int sort_column;
	Gtk::SortType sort_type;
	void toggle_sort_type ();
	void search_string_updated (const std::string&);
	void print () const;

	sigc::connection _refresh_connection;
};

#endif /* __ardour_gtk_key_editor_h__ */
