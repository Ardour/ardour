/*
    Copyright (C) 2000 Paul Davis

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

#ifndef __ardour_mixer_snapshot_dialog_h__
#define __ardour_mixer_snapshot_dialog_h__

#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>

#include "gtkmm2ext/dndtreeview.h"

#include "ardour/mixer_snapshot.h"

#include "ardour_dialog.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

class MixerSnapshotDialog : public ArdourDialog
{
    public:
        MixerSnapshotDialog();
        ~MixerSnapshotDialog();

        void set_session(ARDOUR::Session*);

        int run();

    private:

        void popup_context_menu(int, int64_t, std::string);
        void rename_snapshot(const std::string);
        void remove_snapshot(const std::string);
        bool button_press(GdkEventButton*, bool);
        void new_snapshot(bool);
        void new_snap_from_session(bool);
        bool bootstrap_display_and_model(Gtkmm2ext::DnDTreeView<std::string>&, Glib::RefPtr<Gtk::ListStore>, bool);

        void fav_cell_action(const std::string&, bool);
        void refill();

        struct MixerSnapshotColumns : public Gtk::TreeModel::ColumnRecord {
            MixerSnapshotColumns () {
                add (favorite);
                add (name);
                add (n_tracks);
                add (n_vcas);
                add (n_groups);
                add (has_specials);
                add (date);
                add (version);
                add (timestamp);
                add (full_path);
                add (snapshot);
            }
            Gtk::TreeModelColumn<bool> favorite;
            Gtk::TreeModelColumn<std::string> name;
            Gtk::TreeModelColumn<std::string> version;
            Gtk::TreeModelColumn<int> n_tracks;
            Gtk::TreeModelColumn<int> n_vcas;
            Gtk::TreeModelColumn<int> n_groups;
            Gtk::TreeModelColumn<bool> has_specials;
            Gtk::TreeModelColumn<std::string> date;
            Gtk::TreeModelColumn<int64_t> timestamp;
            Gtk::TreeModelColumn<std::string> full_path;
            Gtk::TreeModelColumn<MixerSnapshot*> snapshot;
        };

        MixerSnapshotColumns _columns;

        Gtkmm2ext::DnDTreeView<std::string> global_display;
        Gtkmm2ext::DnDTreeView<std::string> local_display;
        Gtk::ScrolledWindow global_scroller;
        Gtk::ScrolledWindow local_scroller;
        Glib::RefPtr<Gtk::ListStore> global_model;
        Glib::RefPtr<Gtk::ListStore> local_model;
        Gtk::Menu _menu;

};
#endif
