/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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
#ifndef __gtk_ardour_editor_regions_h__
#define __gtk_ardour_editor_regions_h__

#include <boost/unordered_map.hpp>

#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treerowreference.h>
#include <gtkmm/treestore.h>

#include "editor_component.h"

class EditorRegions : public EditorComponent, public ARDOUR::SessionHandlePtr
{
public:
	EditorRegions (Editor *);

	void set_session (ARDOUR::Session *);

	Gtk::Widget& widget () {
		return _scroller;
	}

	void clear ();

	void set_selected (RegionSelection &);
	void selection_mapover (sigc::slot<void,boost::shared_ptr<ARDOUR::Region> >);

	boost::shared_ptr<ARDOUR::Region> get_dragged_region ();
	boost::shared_ptr<ARDOUR::Region> get_single_selection ();

	void redisplay ();

	void suspend_redisplay () {
		_no_redisplay = true;
	}

	void resume_redisplay () {
		_no_redisplay = false;
		redisplay ();
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
			add (channels);
			add (tags);
			add (start);
			add (length);
			add (end);
			add (sync);
			add (fadein);
			add (fadeout);
			add (locked);
			add (glued);
			add (muted);
			add (opaque);
			add (path);
			add (region);
			add (color_);
			add (position);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<int> channels;
		Gtk::TreeModelColumn<std::string> tags;
		Gtk::TreeModelColumn<Temporal::timepos_t> position;
		Gtk::TreeModelColumn<std::string> start;
		Gtk::TreeModelColumn<std::string> end;
		Gtk::TreeModelColumn<std::string> length;
		Gtk::TreeModelColumn<std::string> sync;
		Gtk::TreeModelColumn<std::string> fadein;
		Gtk::TreeModelColumn<std::string> fadeout;
		Gtk::TreeModelColumn<bool> locked;
		Gtk::TreeModelColumn<bool> glued;
		Gtk::TreeModelColumn<bool> muted;
		Gtk::TreeModelColumn<bool> opaque;
		Gtk::TreeModelColumn<std::string> path;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Region> > region;
		Gtk::TreeModelColumn<Gdk::Color> color_;
	};

	Columns _columns;

	Gtk::TreeModel::RowReference last_row;

	void freeze_tree_model ();
	void thaw_tree_model ();
	void regions_changed (boost::shared_ptr<ARDOUR::RegionList>, PBD::PropertyChange const &);
	void selection_changed ();

	sigc::connection _change_connection;

	int           _sort_col_id;
	Gtk::SortType _sort_type;

	bool selection_filter (const Glib::RefPtr<Gtk::TreeModel>& model, const Gtk::TreeModel::Path& path, bool yn);

	Gtk::Widget* old_focus;

	Gtk::CellEditable* name_editable;
	void name_editing_started (Gtk::CellEditable*, const Glib::ustring&);
	void name_edit (const std::string&, const std::string&);


	Gtk::CellEditable* tags_editable;
	void tag_editing_started (Gtk::CellEditable*, const Glib::ustring&);
	void tag_edit (const std::string&, const std::string&);


	void locked_changed (std::string const &);
	void glued_changed (std::string const &);
	void muted_changed (std::string const &);
	void opaque_changed (std::string const &);

	bool key_press (GdkEventKey *);
	bool button_press (GdkEventButton *);

	bool focus_in (GdkEventFocus*);
	bool focus_out (GdkEventFocus*);
	bool enter_notify (GdkEventCrossing*);
	bool leave_notify (GdkEventCrossing*);

	void show_context_menu (int button, int time);

	void format_position (Temporal::timepos_t const & pos, char* buf, size_t bufsize, bool onoff = true);

	void add_region (boost::shared_ptr<ARDOUR::Region>);
	void destroy_region (boost::shared_ptr<ARDOUR::Region>);

	void populate_row (boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::Row const &, PBD::PropertyChange const &);
	void populate_row_used (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_position (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_end (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_sync (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_fade_in (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row, boost::shared_ptr<ARDOUR::AudioRegion>);
	void populate_row_fade_out (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row, boost::shared_ptr<ARDOUR::AudioRegion>);
	void populate_row_locked (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_muted (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_glued (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_opaque (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_length (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_name (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);
	void populate_row_source (boost::shared_ptr<ARDOUR::Region> region, Gtk::TreeModel::Row const& row);

	void update_row (boost::shared_ptr<ARDOUR::Region>);

	void clock_format_changed ();

	void drag_data_received (
		Glib::RefPtr<Gdk::DragContext> const &, gint, gint, Gtk::SelectionData const &, guint, guint
		);

	Glib::RefPtr<Gtk::Action> remove_unused_regions_action () const;

	Gtk::Menu* _menu;
	Gtk::ScrolledWindow _scroller;
	Gtk::Frame _frame;

	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Region> > _display;

	Glib::RefPtr<Gtk::TreeStore> _model;

	bool _no_redisplay;

	typedef boost::unordered_map<boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::iterator> RegionRowMap;

	RegionRowMap region_row_map;

	PBD::ScopedConnection region_property_connection;
	PBD::ScopedConnection check_new_region_connection;

	PBD::ScopedConnection editor_freeze_connection;
	PBD::ScopedConnection editor_thaw_connection;
};

#endif /* __gtk_ardour_editor_regions_h__ */
