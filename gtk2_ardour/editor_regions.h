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
#ifndef __gtk_ardour_editor_regions_h__
#define __gtk_ardour_editor_regions_h__

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

	void toggle_full ();
	void toggle_show_auto_regions ();
	void reset_sort_direction (bool);
	void reset_sort_type (Editing::RegionListSortType, bool);
	void set_selected (RegionSelection &);
	void selection_mapover (sigc::slot<void,boost::shared_ptr<ARDOUR::Region> >);
	boost::shared_ptr<ARDOUR::Region> get_dragged_region ();
	boost::shared_ptr<ARDOUR::Region> get_single_selection ();
	Editing::RegionListSortType sort_type () const {
		return _sort_type;
	}
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

private:

	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns () {
			add (name);
			add (region);
			add (color_);
			add (start);
			add (end);
			add (length);
			add (sync);
			add (fadein);
			add (fadeout);
			add (locked);
			add (glued);
			add (muted);
			add (opaque);
			add (used);
			add (path);
			add (property_toggles_visible);
		}

		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Region> > region;
		Gtk::TreeModelColumn<Gdk::Color> color_;
		Gtk::TreeModelColumn<Glib::ustring> start;
		Gtk::TreeModelColumn<Glib::ustring> end;
		Gtk::TreeModelColumn<Glib::ustring> length;
		Gtk::TreeModelColumn<Glib::ustring> sync;
		Gtk::TreeModelColumn<Glib::ustring> fadein;
		Gtk::TreeModelColumn<Glib::ustring> fadeout;
		Gtk::TreeModelColumn<bool> locked;
		Gtk::TreeModelColumn<bool> glued;
		Gtk::TreeModelColumn<bool> muted;
		Gtk::TreeModelColumn<bool> opaque;
		Gtk::TreeModelColumn<Glib::ustring> used;
		Gtk::TreeModelColumn<Glib::ustring> path;
		/** used to indicate whether the locked/glued/muted/opaque should be visible or not */
		Gtk::TreeModelColumn<bool> property_toggles_visible;
	};

	Columns _columns;

	void region_changed (boost::shared_ptr<ARDOUR::Region>, PBD::PropertyChange const &);
	void selection_changed ();
	sigc::connection _change_connection;
	bool set_selected_in_subrow (boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::Row const &, int);
	bool selection_filter (const Glib::RefPtr<Gtk::TreeModel>& model, const Gtk::TreeModel::Path& path, bool yn);
	void name_edit (const Glib::ustring&, const Glib::ustring&);
	void locked_changed (Glib::ustring const &);
	void glued_changed (Glib::ustring const &);
	void muted_changed (Glib::ustring const &);
	void opaque_changed (Glib::ustring const &);

	bool key_press (GdkEventKey *);
	bool button_press (GdkEventButton *);
	void build_menu ();
	void show_context_menu (int button, int time);

	int sorter (Gtk::TreeModel::iterator, Gtk::TreeModel::iterator);

	void add_region (boost::shared_ptr<ARDOUR::Region>);
	void add_regions (std::vector<boost::shared_ptr<ARDOUR::Region> > & );
	void populate_row (boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::Row const &);
	void update_row (boost::shared_ptr<ARDOUR::Region>);
	bool update_subrows (boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::Row const &, int);
	void update_all_rows ();
	void update_all_subrows (Gtk::TreeModel::Row const &, int);
	void insert_into_tmp_regionlist (boost::shared_ptr<ARDOUR::Region>);

	void drag_data_received (
		Glib::RefPtr<Gdk::DragContext> const &, gint, gint, Gtk::SelectionData const &, guint, guint
		);

	Gtk::Menu* _menu;
	Gtk::ScrolledWindow _scroller;
	Gtk::Frame _frame;
	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Region> > _display;
	Glib::RefPtr<Gtk::TreeStore> _model;
	Glib::RefPtr<Gtk::Action> _hide_action; ///< the action for our Hide menu option
	Glib::RefPtr<Gtk::Action> _show_action; ///< the action for our Show menu option
	Glib::RefPtr<Gtk::ToggleAction> _toggle_full_action;
	Glib::RefPtr<Gtk::ToggleAction> _toggle_show_auto_regions_action;
	bool _show_automatic_regions;
	Editing::RegionListSortType _sort_type;
	bool _no_redisplay;
	std::list<boost::shared_ptr<ARDOUR::Region> > tmp_region_list;
	PBD::ScopedConnection region_property_connection;
	PBD::ScopedConnection check_new_region_connection;
	bool ignore_region_list_selection_change;
	bool ignore_selected_region_change;
};

#endif /* __gtk_ardour_editor_regions_h__ */
