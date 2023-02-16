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

#include "editor_component.h"
#include "source_list_base.h"

class EditorSources : public EditorComponent, public SourceListBase
{
public:
	EditorSources (Editor*);

	std::shared_ptr<ARDOUR::Region> get_single_selection ();

	/* user actions */
	void remove_selected_sources ();
	void recover_selected_sources ();

private:
	void init ();
	bool key_press (GdkEventKey*);
	bool button_press (GdkEventButton*);
	void show_context_menu (int button, int time);

	void selection_changed ();

	void drag_data_received (Glib::RefPtr<Gdk::DragContext> const&, gint, gint, Gtk::SelectionData const&, guint, guint);
};

#endif
