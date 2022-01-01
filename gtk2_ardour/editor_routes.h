/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_editor_route_h__
#define __ardour_gtk_editor_route_h__

#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>

#include "pbd/signals.h"
#include "gtkmm2ext/widget_state.h"

#include "editor_component.h"

class VCATimeAxisView;

class EditorRoutes : public EditorComponent, public PBD::ScopedConnectionList, public ARDOUR::SessionHandlePtr
{
public:
	EditorRoutes (Editor *);
	~EditorRoutes ();

	void set_session (ARDOUR::Session *);

	Gtk::Widget& widget () {
		return _scroller;
	}

	void move_selected_tracks (bool);
	void show_track_in_display (TimeAxisView &);


	void suspend_redisplay () {
		if (!_no_redisplay) {
			_no_redisplay = true;
			_redisplay_on_resume = false;
		}
	}

	void resume_redisplay () {
		_no_redisplay = false;
		if (_redisplay_on_resume) {
			redisplay ();
		}
	}

	void redisplay ();
	void update_visibility ();
	void time_axis_views_added (std::list<TimeAxisView*>);
	void route_removed (TimeAxisView *);
	void hide_track_in_display (TimeAxisView &);
	std::list<TimeAxisView*> views () const;
	void hide_all_tracks (bool);
	void clear ();
	void sync_presentation_info_from_treeview ();
	void sync_treeview_from_presentation_info (PBD::PropertyChange const &);

private:
	void initial_display ();
	void redisplay_real ();
	void on_input_active_changed (std::string const &);
	void on_tv_rec_enable_changed (std::string const &);
	void on_tv_rec_safe_toggled (std::string const &);
	void on_tv_mute_enable_toggled (std::string const &);
	void on_tv_solo_enable_toggled (std::string const &);
	void on_tv_solo_isolate_toggled (std::string const &);
	void on_tv_solo_safe_toggled (std::string const &);
	void build_menu ();
	void presentation_info_changed (PBD::PropertyChange const &);
	void row_deleted (Gtk::TreeModel::Path const &);
	void visible_changed (std::string const &);
	void trigger_changed (std::string const &);
	void active_changed (std::string const &);
	void reordered (Gtk::TreeModel::Path const &, Gtk::TreeModel::iterator const &, int *);
	bool button_press (GdkEventButton *);
	bool button_release (GdkEventButton *);
	void route_property_changed (const PBD::PropertyChange&, boost::weak_ptr<ARDOUR::Stripable>);
	void handle_gui_changes (std::string const &, void *);
	bool idle_update_mute_rec_solo_etc ();
	void update_rec_display ();
	void update_mute_display ();
	void update_solo_display ();
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
	void selection_changed ();

	int plugin_setup (boost::shared_ptr<ARDOUR::Route>, boost::shared_ptr<ARDOUR::PluginInsert>, ARDOUR::Route::PluginSetupOptions);

	void name_edit (std::string const &, std::string const &);
	void solo_changed_so_update_mute ();

	struct ModelColumns : public Gtk::TreeModel::ColumnRecord {
		ModelColumns() {
			add (text);
			add (visible);
			add (trigger);
			add (rec_state);
			add (rec_safe);
			add (mute_state);
			add (solo_state);
			add (solo_visible);
			add (solo_lock_iso_visible);
			add (solo_isolate_state);
			add (solo_safe_state);
			add (is_track);
			add (tv);
			add (stripable);
			add (name_editable);
			add (is_input_active);
			add (is_midi);
			add (no_vca);
			add (active);
		}

		Gtk::TreeModelColumn<std::string>    text;
		Gtk::TreeModelColumn<bool>           visible;
		Gtk::TreeModelColumn<bool>           trigger;
		Gtk::TreeModelColumn<uint32_t>       rec_state;
		Gtk::TreeModelColumn<uint32_t>       rec_safe;
		Gtk::TreeModelColumn<uint32_t>       mute_state;
		Gtk::TreeModelColumn<uint32_t>       solo_state;
		/** true if the solo buttons are visible for this route, otherwise false */
		Gtk::TreeModelColumn<bool>           solo_visible;
		Gtk::TreeModelColumn<bool>           solo_lock_iso_visible;
		Gtk::TreeModelColumn<uint32_t>       solo_isolate_state;
		Gtk::TreeModelColumn<uint32_t>       solo_safe_state;
		Gtk::TreeModelColumn<bool>           is_track;
		Gtk::TreeModelColumn<TimeAxisView*>  tv;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Stripable> >  stripable;
		Gtk::TreeModelColumn<bool>           name_editable;
		Gtk::TreeModelColumn<bool>           is_input_active;
		Gtk::TreeModelColumn<bool>           is_midi;
		Gtk::TreeModelColumn<bool>           no_vca; // activatable
		Gtk::TreeModelColumn<bool>           active;
	};

	Gtk::TreeViewColumn* rec_state_column;
	Gtk::TreeViewColumn* rec_safe_column;
	Gtk::TreeViewColumn* input_active_column;
	Gtk::TreeViewColumn* mute_state_column;
	Gtk::TreeViewColumn* solo_state_column;
	Gtk::TreeViewColumn* solo_safe_state_column;
	Gtk::TreeViewColumn* solo_isolate_state_column;
	Gtk::TreeViewColumn* name_column;
	Gtk::TreeViewColumn* visible_column;
	Gtk::TreeViewColumn* trigger_column;
	Gtk::TreeViewColumn* active_column;

	Gtk::ScrolledWindow _scroller;
	Gtk::TreeView _display;
	Glib::RefPtr<Gtk::ListStore> _model;
	ModelColumns _columns;
	int _name_column;
	int _visible_column;
	int _trigger_column;
	int _active_column;

	bool _ignore_reorder;
	bool _ignore_selection_change;
	bool column_does_not_select;
	bool _no_redisplay;
	bool _adding_routes;
	bool _route_deletion_in_progress;
	bool _redisplay_on_resume;
	bool _idle_update_queued;

	volatile int _redisplay_active;

	Gtk::Menu* _menu;
	Gtk::Widget* old_focus;
	Gtk::CellEditable* name_editable;

	bool select_function (const Glib::RefPtr<Gtk::TreeModel>& model, const Gtk::TreeModel::Path& path, bool);

	bool key_press (GdkEventKey* ev);
	bool focus_in (GdkEventFocus*);
	bool focus_out (GdkEventFocus*);
	bool enter_notify (GdkEventCrossing*);
	bool leave_notify (GdkEventCrossing*);
	void name_edit_started (Gtk::CellEditable*, const Glib::ustring&);

	bool get_relevant_routes (boost::shared_ptr<ARDOUR::RouteList> rl);
};

#endif /* __ardour_gtk_editor_route_h__ */
