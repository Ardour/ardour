/*
    Copyright (C) 2000-2019 Paul Davis

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

#ifndef __gtk_ardour_mixer_snapshots_h__
#define __gtk_ardour_mixer_snapshots_h__

#include <ctime>

#include <gtkmm/button.h>
#include <gtkmm/widget.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>
#include <gtkmm/box.h>

#include "ardour/mixer_snapshot.h"

class MixerSnapshotList : public ARDOUR::SessionHandlePtr
{
public:
	MixerSnapshotList ();

	void set_session (ARDOUR::Session *);

	Gtk::Widget& widget () {
		return *_window_packer;
	}

	void new_snapshot();
	void new_snapshot_from_session();

	void redisplay ();

private:
	Gtk::VBox* _window_packer;
	Gtk::HBox* _button_packer;

	Gtk::ScrolledWindow _scroller;

	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns () {
			add (name);
			add (timestamp);
            add (snapshot);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::time_t> timestamp;
        Gtk::TreeModelColumn<ARDOUR::MixerSnapshot*> snapshot;  //TODO: these are leaked
	};

	Columns _columns;
	Glib::RefPtr<Gtk::ListStore> _snapshot_model;
	Gtk::TreeView _snapshot_display;
	Gtk::Menu _menu;

	Gtk::Button add_template_button;
	Gtk::Button add_session_template_button;

	bool button_press (GdkEventButton *);
	void selection_changed ();
	void popup_context_menu (int, int32_t, Gtk::TreeModel::iterator&);
	void remove_snapshot (Gtk::TreeModel::iterator&);
	void rename_snapshot (Gtk::TreeModel::iterator&);
	void promote_snapshot (Gtk::TreeModel::iterator&);
};

#endif // __gtk_ardour_mixer_snapshots_h__
