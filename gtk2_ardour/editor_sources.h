/*
    Copyright (C) 2000-2009 Paul Davis

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

	void set_selection (Selection *sel) { _selection = sel; }

	Gtk::Widget& widget () {
		return _scroller;
	}

	void clear ();

	void reset_sort_direction (bool);
	void reset_sort_type (Editing::RegionListSortType, bool);
	void selection_mapover (sigc::slot<void,boost::shared_ptr<ARDOUR::Region> >);

	boost::shared_ptr<ARDOUR::Region> get_dragged_region ();
	boost::shared_ptr<ARDOUR::Region> get_single_selection ();

	Editing::RegionListSortType sort_type () const {
		return _sort_type;
	}

	void block_change_connection (bool b) {
		_change_connection.block (b);
	}

	void unselect_all () {
		_display.get_selection()->unselect_all ();
	}

	void remove_unused_regions ();

	XMLNode& get_state () const;
	void set_state (const XMLNode &);

private:

	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns () {
			add (name);
			add (source);
			add (color_);
			add (natural_pos);
			add (path);
			add (take_id);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Source> > source;
		Gtk::TreeModelColumn<Gdk::Color> color_;
		Gtk::TreeModelColumn<std::string> natural_pos;
		Gtk::TreeModelColumn<std::string> path;
		Gtk::TreeModelColumn<std::string> take_id;
	};

	Columns _columns;

	Gtk::TreeModel::RowReference last_row;

	void freeze_tree_model ();
	void thaw_tree_model ();
	void source_changed (boost::shared_ptr<ARDOUR::Source>);
	void populate_row (Gtk::TreeModel::Row row, boost::shared_ptr<ARDOUR::Source> source);
	void selection_changed ();

	sigc::connection _change_connection;

	bool selection_filter (const Glib::RefPtr<Gtk::TreeModel>& model, const Gtk::TreeModel::Path& path, bool yn);

	Gtk::Widget* old_focus;
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

	int sorter (Gtk::TreeModel::iterator, Gtk::TreeModel::iterator);

	void format_position (ARDOUR::samplepos_t pos, char* buf, size_t bufsize, bool onoff = true);

	void add_source (boost::shared_ptr<ARDOUR::Source>);
	void remove_source (boost::shared_ptr<ARDOUR::Source>);

	void populate_row (boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::Row const &, PBD::PropertyChange const &);
	void populate_row_name (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_source (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);

	void update_row (boost::shared_ptr<ARDOUR::Region>);
	void update_all_rows ();

	void insert_into_tmp_regionlist (boost::shared_ptr<ARDOUR::Region>);

	void drag_data_received (
		Glib::RefPtr<Gdk::DragContext> const &, gint, gint, Gtk::SelectionData const &, guint, guint
		);

	Glib::RefPtr<Gtk::RadioAction> sort_type_action (Editing::RegionListSortType) const;

	Glib::RefPtr<Gtk::Action> hide_action () const;
	Glib::RefPtr<Gtk::Action> show_action () const;
	Glib::RefPtr<Gtk::Action> remove_unused_regions_action () const;

	Gtk::Menu* _menu;
	Gtk::ScrolledWindow _scroller;
	Gtk::Frame _frame;

	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Region> > _display;

	Glib::RefPtr<Gtk::TreeStore> _model;

	bool ignore_region_list_selection_change;
	bool ignore_selected_region_change;

	Editing::RegionListSortType _sort_type;

	std::list<boost::shared_ptr<ARDOUR::Region> > tmp_region_list;

	typedef boost::unordered_map<boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::RowReference> RegionRowMap;
	typedef boost::unordered_map<std::string, Gtk::TreeModel::RowReference > RegionSourceMap;

	RegionRowMap region_row_map;
	RegionSourceMap parent_regions_sources_map;

	PBD::ScopedConnection source_property_connection;

	PBD::ScopedConnection source_added_connection;
	PBD::ScopedConnection source_removed_connection;

	PBD::ScopedConnection editor_freeze_connection;
	PBD::ScopedConnection editor_thaw_connection;

	Selection* _selection;

};

#endif /* __gtk_ardour_editor_regions_h__ */
