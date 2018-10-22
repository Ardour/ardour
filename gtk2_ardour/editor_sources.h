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
	
	void remove_selected_sources ();

	void selection_mapover (sigc::slot<void,boost::shared_ptr<ARDOUR::Region> >);

	boost::shared_ptr<ARDOUR::Source> get_dragged_source ();
	boost::shared_ptr<ARDOUR::Source> get_single_selection ();

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
			add (take_id);
			add (natural_pos);
			add (path);
			add (color_);
			add (source);
			add (natural_s);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Source> > source;
		Gtk::TreeModelColumn<Gdk::Color> color_;
		Gtk::TreeModelColumn<std::string> natural_pos;
		Gtk::TreeModelColumn<std::string> path;
		Gtk::TreeModelColumn<std::string> take_id;
		Gtk::TreeModelColumn<samplepos_t> natural_s;
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

	bool key_press (GdkEventKey *);
	bool button_press (GdkEventButton *);

	bool focus_in (GdkEventFocus*);
	bool focus_out (GdkEventFocus*);
	bool enter_notify (GdkEventCrossing*);
	bool leave_notify (GdkEventCrossing*);

	void show_context_menu (int button, int time);

	void format_position (ARDOUR::samplepos_t pos, char* buf, size_t bufsize, bool onoff = true);

	void add_source (boost::shared_ptr<ARDOUR::Source>);
	void remove_source (boost::shared_ptr<ARDOUR::Source>);

	void clock_format_changed ();

	void drag_data_received (
		Glib::RefPtr<Gdk::DragContext> const &, gint, gint, Gtk::SelectionData const &, guint, guint
		);

	Glib::RefPtr<Gtk::Action> remove_unused_regions_action () const;  //TODO: what is the equivalent?

	Gtk::Menu* _menu;
	Gtk::ScrolledWindow _scroller;
	Gtk::Frame _frame;

	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Source> > _display;  //TODO .. try changing this to region

	Glib::RefPtr<Gtk::TreeStore> _model;

	PBD::ScopedConnection source_property_connection;

	PBD::ScopedConnection source_added_connection;
	PBD::ScopedConnection source_removed_connection;

	PBD::ScopedConnection editor_freeze_connection;
	PBD::ScopedConnection editor_thaw_connection;

	Selection* _selection;

};

#endif /* __gtk_ardour_editor_regions_h__ */
