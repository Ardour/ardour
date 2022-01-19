/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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
#ifndef _gtk_ardour_region_list_base_h_
#define _gtk_ardour_region_list_base_h_

#include <boost/unordered_map.hpp>

#include <gtkmm/celleditable.h>
#include <gtkmm/frame.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treerowreference.h>
#include <gtkmm/treestore.h>

#include "gtkmm2ext/utils.h"

#include "pbd/properties.h"
#include "pbd/signals.h"

#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "gtkmm2ext/dndtreeview.h"

//#define SHOW_REGION_EXTRAS

namespace ARDOUR
{
	class Region;
	class AudioRegion;
}

class RegionListBase : public ARDOUR::SessionHandlePtr
{
public:
	RegionListBase ();

	void set_session (ARDOUR::Session*);

	Gtk::Widget& widget ()
	{
		return _scroller;
	}

	void clear ();

	void redisplay ();

	void suspend_redisplay ()
	{
		_no_redisplay = true;
	}

	void resume_redisplay ()
	{
		_no_redisplay = false;
		redisplay ();
	}

	void block_change_connection (bool b)
	{
		_change_connection.block (b);
	}

	void unselect_all ()
	{
		_display.get_selection ()->unselect_all ();
	}

protected:
	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns ()
		{
			add (name);     // 0
			add (channels); // 1
			add (tags);     // 2
			add (start);    // 3
			add (length);   // 3
			add (end);      // 5
			add (sync);     // 6
			add (fadein);   // 7
			add (fadeout);  // 8
			add (locked);   // 9
			add (glued);    // 10
			add (muted);    // 11
			add (opaque);   // 12
			add (path);     // 13
			add (region);   // 14
			add (color_);   // 15
			add (position); // 16
			/* src-list */
			add (captd_for);   // 17
			add (take_id);     // 18
			add (natural_pos); // 19
			add (natural_s);   // 20
			add (captd_xruns);
		}

		Gtk::TreeModelColumn<std::string>                       name;
		Gtk::TreeModelColumn<int>                               channels;
		Gtk::TreeModelColumn<std::string>                       tags;
		Gtk::TreeModelColumn<Temporal::timepos_t>               position;
		Gtk::TreeModelColumn<std::string>                       start;
		Gtk::TreeModelColumn<std::string>                       end;
		Gtk::TreeModelColumn<std::string>                       length;
		Gtk::TreeModelColumn<std::string>                       sync;
		Gtk::TreeModelColumn<std::string>                       fadein;
		Gtk::TreeModelColumn<std::string>                       fadeout;
		Gtk::TreeModelColumn<bool>                              locked;
		Gtk::TreeModelColumn<bool>                              glued;
		Gtk::TreeModelColumn<bool>                              muted;
		Gtk::TreeModelColumn<bool>                              opaque;
		Gtk::TreeModelColumn<std::string>                       path;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Region>> region;
		Gtk::TreeModelColumn<Gdk::Color>                        color_;
		Gtk::TreeModelColumn<std::string>                       captd_for;
		Gtk::TreeModelColumn<std::string>                       take_id;
		Gtk::TreeModelColumn<std::string>                       natural_pos;
		Gtk::TreeModelColumn<Temporal::timepos_t>               natural_s;
		Gtk::TreeModelColumn<size_t>                            captd_xruns;
	};

	void add_name_column ();
	void add_tag_column ();

	template <class T>
	Gtk::TreeViewColumn* append_col (Gtk::TreeModelColumn<T> const& col, int width)
	{
		Gtk::TreeViewColumn* c = manage (new Gtk::TreeViewColumn ("", col));
		c->set_fixed_width (width);
		c->set_sizing (Gtk::TREE_VIEW_COLUMN_FIXED);
		_display.append_column (*c);
		return c;
	}

	template <class T>
	Gtk::TreeViewColumn* append_col (Gtk::TreeModelColumn<T> const& col, std::string const& sizing_text)
	{
		int w, h;
		Glib::RefPtr<Pango::Layout> layout = _display.create_pango_layout (sizing_text);
		Gtkmm2ext::get_pixel_size (layout, w, h);
		return append_col (col, w);
	}

	void setup_col (Gtk::TreeViewColumn*, int, Gtk::AlignmentEnum, const char*, const char*);
	void setup_toggle (Gtk::TreeViewColumn*, sigc::slot<void, std::string>);

	void freeze_tree_model ();
	void thaw_tree_model ();
	void remove_weak_region (boost::weak_ptr<ARDOUR::Region>);

	virtual void regions_changed (boost::shared_ptr<ARDOUR::RegionList>, PBD::PropertyChange const&);

	void name_editing_started (Gtk::CellEditable*, const Glib::ustring&);
	void tag_editing_started (Gtk::CellEditable*, const Glib::ustring&);

	virtual void name_edit (const std::string&, const std::string&);
	virtual void tag_edit (const std::string&, const std::string&);

	void locked_changed (std::string const&);
	void glued_changed (std::string const&);
	void muted_changed (std::string const&);
	void opaque_changed (std::string const&);

	virtual bool key_press (GdkEventKey*);
	virtual bool button_press (GdkEventButton*)
	{
		return false;
	}

	bool focus_in (GdkEventFocus*);
	bool focus_out (GdkEventFocus*);

	bool enter_notify (GdkEventCrossing*);
	bool leave_notify (GdkEventCrossing*);

	void format_position (Temporal::timepos_t const& pos, char* buf, size_t bufsize, bool onoff = true);

	void add_region (boost::shared_ptr<ARDOUR::Region>);

	void populate_row (boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::Row const&, PBD::PropertyChange const&);
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

	void clock_format_changed ();

	void drag_begin (Glib::RefPtr<Gdk::DragContext> const&);
	void drag_end (Glib::RefPtr<Gdk::DragContext> const&);
	void drag_data_get (Glib::RefPtr<Gdk::DragContext> const&, Gtk::SelectionData&, guint, guint);

	virtual bool list_region (boost::shared_ptr<ARDOUR::Region>) const;

	Columns _columns;

	int           _sort_col_id;
	Gtk::SortType _sort_type;

	Gtk::CellEditable* _name_editable;
	Gtk::CellEditable* _tags_editable;
	Gtk::Widget*       _old_focus;

	Gtk::ScrolledWindow _scroller;
	Gtk::Frame          _frame;

	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Region>> _display;

	Glib::RefPtr<Gtk::TreeStore> _model;

	bool _no_redisplay;

	typedef boost::unordered_map<boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::iterator> RegionRowMap;

	RegionRowMap region_row_map;

	sigc::connection _change_connection;

	PBD::ScopedConnection _editor_freeze_connection;
	PBD::ScopedConnection _editor_thaw_connection;

	PBD::ScopedConnectionList _remove_region_connections;
};

#endif /* _gtk_ardour_region_list_base_h_ */
