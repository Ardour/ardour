/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#ifndef _gtk_ardour_editor_sections_h_
#define _gtk_ardour_editor_sections_h_

#include "ardour/location.h"
#include "ardour/session_handle.h"

#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>

class EditorSections : public ARDOUR::SessionHandlePtr, public virtual sigc::trackable
{
public:
	EditorSections ();

	void set_session (ARDOUR::Session*);

	Gtk::Widget& widget ()
	{
		return _scroller;
	}

private:
	void redisplay ();
	bool delete_selected_section ();
	bool rename_selected_section ();

	void clear_selection ();
	void selection_changed ();
	void clock_format_changed ();
	bool scroll_row_timeout ();
	void show_context_menu (int, int);

	bool key_press (GdkEventKey*);
	bool button_press (GdkEventButton*);
	bool enter_notify (GdkEventCrossing*);
	bool leave_notify (GdkEventCrossing*);

	void drag_data_get (Glib::RefPtr<Gdk::DragContext> const&, Gtk::SelectionData&, guint, guint);
	void drag_begin (Glib::RefPtr<Gdk::DragContext> const&);
	bool drag_motion (Glib::RefPtr<Gdk::DragContext> const&, int, int, guint);
	void drag_data_received (Glib::RefPtr<Gdk::DragContext> const&, int, int, Gtk::SelectionData const&, guint, guint);
	void drag_leave (Glib::RefPtr<Gdk::DragContext> const&, guint);

	void name_edited (const std::string&, const std::string&);

	struct Section {
		Section ()
		    : location (NULL)
		    , start (0)
		    , end (0)
		{
		}

		Section (ARDOUR::Location const* const l, Temporal::timepos_t const& s, Temporal::timepos_t const& e)
		    : location (l)
		    , start (s)
		    , end (e)
		{
		}

		ARDOUR::Location const* const location;
		Temporal::timepos_t const     start;
		Temporal::timepos_t const     end;
	};

	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns ()
		{
			add (name);
			add (s_start);
			add (s_end);
			add (location);
			add (start);
			add (end);
		}
		Gtk::TreeModelColumn<std::string>         name;
		Gtk::TreeModelColumn<std::string>         s_start;
		Gtk::TreeModelColumn<std::string>         s_end;
		Gtk::TreeModelColumn<ARDOUR::Location*>   location;
		Gtk::TreeModelColumn<Temporal::timepos_t> start;
		Gtk::TreeModelColumn<Temporal::timepos_t> end;
	};

	Columns                      _columns;
	Glib::RefPtr<Gtk::ListStore> _model;
	Gtk::TreeView                _view;
	Gtk::ScrolledWindow          _scroller;

	bool             _no_redisplay;
	sigc::connection _scroll_timeout;
	sigc::connection _selection_change;
};

#endif
