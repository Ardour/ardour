/*
    Copyright (C) 2019 Nikolaus Gullotta

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

#include "ardour_window.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

class MixerSnapshotDialog : public ArdourWindow
{
    public:
        MixerSnapshotDialog(ARDOUR::Session*);
        ~MixerSnapshotDialog() {};

        void set_session(ARDOUR::Session*);
        void refill();

    private:
        void refill_display(bool);
        void display_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& data, guint info, guint time, bool);

        bool ensure_directory(bool);
        void new_snapshot(bool);
        void load_snapshot(Gtk::TreeModel::iterator&);
        void new_snapshot_from_session(bool);

        void rename_snapshot(Gtk::TreeModel::iterator&);
        void remove_snapshot(Gtk::TreeModel::iterator&, bool);

        bool bootstrap_display_and_model(Gtkmm2ext::DnDTreeView<std::string>&, Gtk::ScrolledWindow&, Glib::RefPtr<Gtk::ListStore>, bool);

        void popup_context_menu(int, int64_t, Gtk::TreeModel::iterator&, bool);
        bool button_press(GdkEventButton*, bool);
        void fav_cell_action(const std::string&, bool);
        void recall_flag_cell_action(const std::string&, bool, std::string);

        void new_row(Glib::RefPtr<Gtk::ListStore>, ARDOUR::MixerSnapshot*, std::string);

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
                add (recall_eq);
                add (recall_comp);
                add (recall_sends);
                add (recall_pan);
                add (recall_plugins);
                add (recall_groups);
                add (recall_vcas);
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
            Gtk::TreeModelColumn<ARDOUR::MixerSnapshot*> snapshot;
            Gtk::TreeModelColumn<bool> recall_eq;
            Gtk::TreeModelColumn<bool> recall_comp;
            Gtk::TreeModelColumn<bool> recall_sends;
            Gtk::TreeModelColumn<bool> recall_pan;
            Gtk::TreeModelColumn<bool> recall_plugins;
            Gtk::TreeModelColumn<bool> recall_groups;
            Gtk::TreeModelColumn<bool> recall_vcas;
        };

        std::string global_snap_path;
        std::string local_snap_path;

        MixerSnapshotColumns _columns;

        Gtkmm2ext::DnDTreeView<std::string> global_display;
        Gtkmm2ext::DnDTreeView<std::string> local_display;
        Gtk::ScrolledWindow global_scroller;
        Gtk::ScrolledWindow local_scroller;
        Glib::RefPtr<Gtk::ListStore> global_model;
        Glib::RefPtr<Gtk::ListStore> local_model;
        Gtk::Menu menu;
        Gtk::VBox top_level_view_box;
};
#endif /* __ardour_mixer_snapshot_dialog_h__ */
