/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __ardour_gtk_editor_route_h__
#define __ardour_gtk_editor_route_h__

#include "pbd/signals.h"
#include "gtkmm2ext/widget_state.h"
#include "editor_component.h"

class EditorRoutes : public EditorComponent, public PBD::ScopedConnectionList, public ARDOUR::SessionHandlePtr
{
public:
	EditorRoutes (Editor *);

	void set_session (ARDOUR::Session *);

	Gtk::Widget& widget () {
		return _scroller;
	}

	void move_selected_tracks (bool);
	void show_track_in_display (TimeAxisView &);

	void suspend_redisplay () {
		_no_redisplay = true;
	}

        void allow_redisplay () { 
		_no_redisplay = false;
	}

	void resume_redisplay () {
		_no_redisplay = false;
		redisplay ();
	}

	void redisplay ();
	void update_visibility ();
	void routes_added (std::list<RouteTimeAxisView*> routes);
	void route_removed (TimeAxisView *);
	void hide_track_in_display (TimeAxisView &);
	std::list<TimeAxisView*> views () const;
	void hide_all_tracks (bool);
	void clear ();
        void sync_order_keys_from_treeview ();
        void reset_remote_control_ids ();

private:
	void initial_display ();
	void on_input_active_changed (std::string const &);
	void on_tv_rec_enable_changed (std::string const &);
	void on_tv_mute_enable_toggled (std::string const &);
	void on_tv_solo_enable_toggled (std::string const &);
	void on_tv_solo_isolate_toggled (std::string const &);
	void on_tv_solo_safe_toggled (std::string const &);
	void build_menu ();
	void show_menu ();
        void sync_treeview_from_order_keys (ARDOUR::RouteSortOrderKey);
	void route_deleted (Gtk::TreeModel::Path const &);
	void visible_changed (std::string const &);
	void active_changed (std::string const &);
	void reordered (Gtk::TreeModel::Path const &, Gtk::TreeModel::iterator const &, int *);
	bool button_press (GdkEventButton *);
	void route_property_changed (const PBD::PropertyChange&, boost::weak_ptr<ARDOUR::Route>);
	void handle_gui_changes (std::string const &, void *);
	void update_rec_display ();
	void update_mute_display ();
	void update_solo_display (bool);
	void update_solo_isolate_display ();
	void update_solo_safe_display ();
	void update_input_active_display ();
	void update_active_display ();
	void set_all_tracks_visibility (bool);
	void set_all_audio_midi_visibility (int, bool);
	void show_all_routes ();
	void hide_all_routes ();
	void show_all_audiotracks ();
	void hide_all_audiotracks ();
	void show_all_audiobus ();
	void hide_all_audiobus ();
	void show_all_miditracks ();
	void hide_all_miditracks ();
	void show_tracks_with_regions_at_playhead ();

	void display_drag_data_received (
		Glib::RefPtr<Gdk::DragContext> const &, gint, gint, Gtk::SelectionData const &, guint, guint
		);

	bool selection_filter (Glib::RefPtr<Gtk::TreeModel> const &, Gtk::TreeModel::Path const &, bool);
	void name_edit (std::string const &, std::string const &);
	void solo_changed_so_update_mute ();

	struct ModelColumns : public Gtk::TreeModel::ColumnRecord {
		ModelColumns() {
			add (text);
			add (visible);
			add (rec_state);
			add (mute_state);
			add (solo_state);
			add (solo_visible);
			add (solo_isolate_state);
			add (solo_safe_state);
			add (is_track);
			add (tv);
			add (route);
			add (name_editable);
			add (is_input_active);
			add (is_midi);
			add (active);
		}

		Gtk::TreeModelColumn<std::string>    text;
		Gtk::TreeModelColumn<bool>           visible;
		Gtk::TreeModelColumn<uint32_t>       rec_state;
		Gtk::TreeModelColumn<uint32_t>       mute_state;
		Gtk::TreeModelColumn<uint32_t>       solo_state;
		/** true if the solo buttons are visible for this route, otherwise false */
		Gtk::TreeModelColumn<bool>           solo_visible;
		Gtk::TreeModelColumn<uint32_t>       solo_isolate_state;
		Gtk::TreeModelColumn<uint32_t>       solo_safe_state;
		Gtk::TreeModelColumn<bool>           is_track;
		Gtk::TreeModelColumn<TimeAxisView*>  tv;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Route> >  route;
		Gtk::TreeModelColumn<bool>           name_editable;
		Gtk::TreeModelColumn<bool>           is_input_active;
		Gtk::TreeModelColumn<bool>           is_midi;
		Gtk::TreeModelColumn<bool>           active;
	};

	Gtk::ScrolledWindow _scroller;
	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Route> > _display;
	Glib::RefPtr<Gtk::ListStore> _model;
	ModelColumns _columns;
	int _name_column;
	int _visible_column;
	int _active_column;

	bool _ignore_reorder;
	bool _no_redisplay;

	Gtk::Menu* _menu;
        Gtk::Widget* old_focus;
        uint32_t selection_countdown;
        Gtk::CellEditable* name_editable;

        bool key_press (GdkEventKey* ev);
        bool focus_in (GdkEventFocus*);
        bool focus_out (GdkEventFocus*);
        bool enter_notify (GdkEventCrossing*);
        bool leave_notify (GdkEventCrossing*);
        void name_edit_started (Gtk::CellEditable*, const Glib::ustring&);

        bool get_relevant_routes (boost::shared_ptr<ARDOUR::RouteList> rl);
};

#endif /* __ardour_gtk_editor_route_h__ */
