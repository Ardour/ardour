/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_trigger_clip_picker_h__
#define __gtk_ardour_trigger_clip_picker_h__

#include <string>

#include <gtkmm/box.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>

#include "widgets/ardour_dropdown.h"

class TriggerClipPicker : public Gtk::VBox
{
public:
	TriggerClipPicker ();
	~TriggerClipPicker ();

private:
	void list_dir (std::string const&);
	void open_dir ();
	void row_selected ();
	void row_activated (Gtk::TreeModel::Path const&, Gtk::TreeViewColumn*);
	void drag_data_get (Glib::RefPtr<Gdk::DragContext> const&, Gtk::SelectionData&, guint, guint);
	void maybe_add_dir (std::string const&);

	ArdourWidgets::ArdourDropdown _dir;
	Gtk::FileChooserDialog        _fcd;

	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns ()
		{
			add (name);
			add (path);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
	};

	Columns                      _columns;
	Glib::RefPtr<Gtk::TreeStore> _model;
	Gtk::TreeView                _view;
	Gtk::ScrolledWindow          _scroller;
};

#endif
