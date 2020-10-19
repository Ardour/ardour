/*
 * Copyright (C) 2018-2019 Ben Loftis <ben@harrisonconsoles.com>
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
#ifndef __gtk_ardour_editor_sources_h__
#define __gtk_ardour_editor_sources_h__

#include <boost/unordered_map.hpp>

#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treerowreference.h>
#include <gtkmm/treestore.h>

#include "editor_component.h"

#include "selection.h"

class EditorSources : public EditorComponent, public ARDOUR::SessionHandlePtr
{
public:
	EditorSources (Editor *);

	void set_session (ARDOUR::Session *);

	Gtk::Widget& widget () {
		return _scroller;
	}

	void clear ();

	boost::shared_ptr<ARDOUR::Region> get_dragged_region ();
	boost::shared_ptr<ARDOUR::Region> get_single_selection ();

	void unselect_all () {
		_display.get_selection()->unselect_all ();
	}

	/* user actions */
	void remove_selected_sources ();
	void recover_selected_sources();

	XMLNode& get_state () const;
	void set_state (const XMLNode &);

private:

	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns () {
			add (name);
			add (channels);
			add (captd_for);
			add (tags);
			add (take_id);
			add (natural_pos);
			add (path);
			add (color_);
			add (region);
			add (natural_s);
			add (captd_xruns);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<int> channels;
		Gtk::TreeModelColumn<std::string> captd_for;
		Gtk::TreeModelColumn<std::string> tags;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Region> > region;
		Gtk::TreeModelColumn<Gdk::Color> color_;
		Gtk::TreeModelColumn<std::string> natural_pos;
		Gtk::TreeModelColumn<std::string> path;
		Gtk::TreeModelColumn<std::string> take_id;
		Gtk::TreeModelColumn<Temporal::timepos_t> natural_s;
		Gtk::TreeModelColumn<size_t> captd_xruns;
	};

	Columns _columns;

	Gtk::TreeModel::RowReference last_row;

	void freeze_tree_model ();
	void thaw_tree_model ();
	void regions_changed (boost::shared_ptr<ARDOUR::RegionList>, PBD::PropertyChange const&);
	void populate_row (Gtk::TreeModel::Row row, boost::shared_ptr<ARDOUR::Region> region);
	void selection_changed ();

	sigc::connection _change_connection;

	int           _sort_col_id;
	Gtk::SortType _sort_type;

	Gtk::Widget* old_focus;

	Gtk::CellEditable* tags_editable;
	void tag_editing_started (Gtk::CellEditable*, const Glib::ustring&);
	void tag_edit (const std::string&, const std::string&);

	Gtk::CellEditable* name_editable;
	void name_editing_started (Gtk::CellEditable*, const Glib::ustring&);
	void name_edit (const std::string&, const std::string&);

	bool key_press (GdkEventKey *);
	bool button_press (GdkEventButton *);

	bool focus_in (GdkEventFocus*);
	bool focus_out (GdkEventFocus*);
	bool enter_notify (GdkEventCrossing*);
	bool leave_notify (GdkEventCrossing*);

	void show_context_menu (int button, int time);

	void format_position (Temporal::timepos_t const & pos, char* buf, size_t bufsize, bool onoff = true);

	void add_source (boost::shared_ptr<ARDOUR::Region>);
	void remove_source (boost::shared_ptr<ARDOUR::Source>);
	void remove_weak_region (boost::weak_ptr<ARDOUR::Region>);
	void remove_weak_source (boost::weak_ptr<ARDOUR::Source>);

	void clock_format_changed ();

	void redisplay ();

	void drag_data_received (
		Glib::RefPtr<Gdk::DragContext> const &, gint, gint, Gtk::SelectionData const &, guint, guint
		);

	Gtk::ScrolledWindow _scroller;

	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Region> > _display;

	Glib::RefPtr<Gtk::TreeStore> _model;

	PBD::ScopedConnection source_property_connection;
	PBD::ScopedConnection add_source_connection;
	PBD::ScopedConnection remove_source_connection;
	PBD::ScopedConnectionList remove_region_connections;

	PBD::ScopedConnection editor_freeze_connection;
	PBD::ScopedConnection editor_thaw_connection;
};

#endif
