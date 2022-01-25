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

#ifndef _ardour_gtk_route_list_base_h_
#define _ardour_gtk_route_list_base_h_

#include <set>

#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>

#include "pbd/properties.h"
#include "pbd/signals.h"

#include "ardour/route.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "gtkmm2ext/cell_renderer_pixbuf_multi.h"

class TimeAxisView;

class RouteListBase : public ARDOUR::SessionHandlePtr
{
public:
	RouteListBase ();
	~RouteListBase ();

	void set_session (ARDOUR::Session*);

	Gtk::Widget& widget ()
	{
		return _scroller;
	}

	void clear ();

protected:
	void add_name_column ();
	void append_col_rec_enable ();
	void append_col_rec_safe ();
	void append_col_input_active ();
	void append_col_mute ();
	void append_col_solo ();

	void setup_col (Gtk::TreeViewColumn*, const char*, const char*);

	template <class T, class U>
	Gtk::TreeViewColumn* append_toggle (Gtk::TreeModelColumn<T> const& col_state, Gtk::TreeModelColumn<U> const& col_viz, sigc::slot<void, std::string> cb)
	{
		Gtk::TreeViewColumn* tvc = manage (new Gtk::TreeViewColumn ("", col_state));
		tvc->set_fixed_width (30);
		tvc->set_sizing (Gtk::TREE_VIEW_COLUMN_FIXED);
		tvc->set_expand (false);
		tvc->set_alignment (Gtk::ALIGN_CENTER);

		Gtk::CellRendererToggle* tc = dynamic_cast<Gtk::CellRendererToggle*> (tvc->get_first_cell_renderer ());
		tc->property_activatable () = true;
		tc->property_radio ()       = false;
		tc->signal_toggled ().connect (cb);

		tvc->add_attribute (tc->property_visible (), col_viz);

		_display.append_column (*tvc);
		no_select_columns.insert (tvc);
		return tvc;
	}

	template <class T, class U>
	Gtkmm2ext::CellRendererPixbufMulti* append_cell (const char* lbl, const char* tip, Gtk::TreeModelColumn<T> const& col_state, Gtk::TreeModelColumn<U> const& col_viz, sigc::slot<void, std::string> cb)
	{
		Gtkmm2ext::CellRendererPixbufMulti* cell;
		Gtk::TreeViewColumn*                tvc;

		cell = manage (new Gtkmm2ext::CellRendererPixbufMulti ());
		cell->signal_changed ().connect (cb);

		tvc = manage (new Gtk::TreeViewColumn (lbl, *cell));
		tvc->add_attribute (cell->property_state (), col_state);
		tvc->add_attribute (cell->property_visible (), col_viz);
		tvc->set_sizing (Gtk::TREE_VIEW_COLUMN_FIXED);
		tvc->set_alignment (Gtk::ALIGN_CENTER);
		tvc->set_expand (false);
		tvc->set_fixed_width (24);

		setup_col (tvc, lbl, tip);
		_display.append_column (*tvc);
		no_select_columns.insert (tvc);
		return cell;
	}

	void on_tv_input_active_changed (std::string const&);
	void on_tv_rec_enable_changed (std::string const&);
	void on_tv_rec_safe_toggled (std::string const&);
	void on_tv_mute_enable_toggled (std::string const&);
	void on_tv_solo_enable_toggled (std::string const&);
	void on_tv_solo_isolate_toggled (std::string const&);
	void on_tv_solo_safe_toggled (std::string const&);
	void on_tv_visible_changed (std::string const&);
	void on_tv_trigger_changed (std::string const&);
	void on_tv_active_changed (std::string const&);

	struct ModelColumns : public Gtk::TreeModel::ColumnRecord {
		ModelColumns ()
		{
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
			add (stripable);
			add (name_editable);
			add (is_input_active);
			add (is_midi);
			add (activatable);
			add (active);
			add (noop_true);
		}

		Gtk::TreeModelColumn<std::string>                          text;
		Gtk::TreeModelColumn<bool>                                 visible;
		Gtk::TreeModelColumn<bool>                                 trigger;
		Gtk::TreeModelColumn<uint32_t>                             rec_state;
		Gtk::TreeModelColumn<uint32_t>                             rec_safe;
		Gtk::TreeModelColumn<uint32_t>                             mute_state;
		Gtk::TreeModelColumn<uint32_t>                             solo_state;
		Gtk::TreeModelColumn<bool>                                 solo_visible; // true if the solo buttons are visible for this route, otherwise false
		Gtk::TreeModelColumn<bool>                                 solo_lock_iso_visible;
		Gtk::TreeModelColumn<uint32_t>                             solo_isolate_state;
		Gtk::TreeModelColumn<uint32_t>                             solo_safe_state;
		Gtk::TreeModelColumn<bool>                                 is_track;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Stripable>> stripable;
		Gtk::TreeModelColumn<bool>                                 name_editable;
		Gtk::TreeModelColumn<bool>                                 is_input_active;
		Gtk::TreeModelColumn<bool>                                 is_midi;
		Gtk::TreeModelColumn<bool>                                 activatable;
		Gtk::TreeModelColumn<bool>                                 active;
		Gtk::TreeModelColumn<bool>                                 noop_true; // always true
	};

	Gtk::TreeView _display;
	ModelColumns  _columns;

private:

	void redisplay ();
	void initial_display ();
	void sync_presentation_info_from_treeview ();
	void sync_treeview_from_presentation_info (PBD::PropertyChange const&);

	void add_routes (ARDOUR::RouteList&);
	void add_masters (ARDOUR::VCAList&);
	void add_stripables (ARDOUR::StripableList&);
	void remove_strip (boost::weak_ptr<ARDOUR::Stripable>);

	void selection_changed ();
	void row_deleted (Gtk::TreeModel::Path const&);
	void reordered (Gtk::TreeModel::Path const&, Gtk::TreeModel::iterator const&, int*);

	bool button_press (GdkEventButton*);
	bool button_release (GdkEventButton*);
	void build_menu ();
	void set_all_audio_midi_visibility (int, bool);
	void show_tracks_with_regions_at_playhead ();

	void queue_idle_update ();
	bool idle_update_mute_rec_solo_etc ();
	void update_input_active_display ();

	void route_property_changed (const PBD::PropertyChange&, boost::weak_ptr<ARDOUR::Stripable>);
	void presentation_info_changed (PBD::PropertyChange const&);

	void name_edit (std::string const&, std::string const&);

	bool select_function (const Glib::RefPtr<Gtk::TreeModel>& model, const Gtk::TreeModel::Path& path, bool);

	bool key_press (GdkEventKey* ev);
	bool focus_in (GdkEventFocus*);
	bool focus_out (GdkEventFocus*);
	bool enter_notify (GdkEventCrossing*);
	bool leave_notify (GdkEventCrossing*);
	void name_edit_started (Gtk::CellEditable*, const Glib::ustring&);

	bool get_relevant_routes (boost::shared_ptr<ARDOUR::RouteList> rl);

	Gtk::ScrolledWindow          _scroller;
	Glib::RefPtr<Gtk::ListStore> _model;

	Gtk::Menu*         _menu;
	Gtk::Widget*       old_focus;
	Gtk::CellEditable* name_editable;

	std::set<Gtk::TreeViewColumn*> no_select_columns;

	bool _ignore_reorder;
	bool _ignore_visibility_change;
	bool _ignore_selection_change;
	bool _column_does_not_select;
	bool _adding_routes;
	bool _route_deletion_in_progress;

	sigc::connection          _idle_update_connection;
	PBD::ScopedConnectionList _stripable_connections;
};

#endif /* _ardour_gtk_route_list_base_h_ */
