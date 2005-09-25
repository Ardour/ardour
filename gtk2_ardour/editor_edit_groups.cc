/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#include <cstdlib>
#include <cmath>

#include <gtkmm2ext/stop_signal.h>
#include <ardour/route_group.h>

#include "editor.h"
#include "keyboard.h"
#include "marker.h"
#include "time_axis_view.h"
#include "prompter.h"

#include <ardour/route.h>

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;

void
Editor::edit_group_list_column_click (gint col)

{
	if (edit_group_list_menu == 0) {
		build_edit_group_list_menu ();
	}

	edit_group_list_menu->popup (0, 0);
}

void
Editor::build_edit_group_list_menu ()

{
	using namespace Gtk::Menu_Helpers;

	edit_group_list_menu = new Menu;
	edit_group_list_menu->set_name ("ArdourContextMenu");
	MenuList& items = edit_group_list_menu->items();

	items.push_back (MenuElem (_("Show All"), mem_fun(*this, &Editor::select_all_edit_groups)));
	items.push_back (MenuElem (_("Hide All"), mem_fun(*this, &Editor::unselect_all_edit_groups)));
}

void
Editor::unselect_all_edit_groups ()

{
}

void
Editor::select_all_edit_groups ()

{
	CList_Helpers::RowList::iterator i;

	/* XXX potential race with remove_track(), but the select operation
	   cannot be done with the track_lock held.
	*/

	for (i = route_list.rows().begin(); i != route_list.rows().end(); ++i) {
		i->select ();
	}
}

void
Editor::new_edit_group ()

{
	if (session == 0) {
		return;
	}

	ArdourPrompter prompter;
	string result;

	prompter.set_prompt (_("Name for new edit group"));
	prompter.done.connect (Gtk::Main::quit.slot());

	prompter.show_all ();
	
	Gtk::Main::run ();
	
	if (prompter.status != Gtkmm2ext::Prompter::entered) {
		return;
	}
	
	prompter.get_result (result);

	if (result.length()) {
		session->add_edit_group (result);
	}
}

void
Editor::edit_group_list_button_clicked ()
{
	new_edit_group ();
}

gint
Editor::edit_group_list_button_press_event (GdkEventButton* ev)
{
	gint row, col;

	if (edit_group_list.get_selection_info ((int)ev->x, (int)ev->y, &row, &col) == 0) {
		return FALSE;
	}

	if (col == 1) {

		if (Keyboard::is_edit_event (ev)) {
			// RouteGroup* group = (RouteGroup *) edit_group_list.row(row).get_data ();
			// edit_route_group (group);

			return stop_signal (edit_group_list, "button_press_event");

		} else {
			/* allow regular select to occur */
			return FALSE;
		}

	} else if (col == 0) {

		RouteGroup* group = reinterpret_cast<RouteGroup *>(edit_group_list.row(row).get_data ());

		if (group) {
			group->set_active (!group->is_active(), this);
		}
	}
	
	return stop_signal (edit_group_list, "button_press_event");
}

void
Editor::edit_group_selected (gint row, gint col, GdkEvent* ev)
{
	RouteGroup* group = (RouteGroup *) edit_group_list.row(row).get_data ();

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if ((*i)->edit_group() == group) {
			select_strip_in_display (*(*i));
		}
	}
}

void
Editor::edit_group_unselected (gint row, gint col, GdkEvent* ev)
{
	RouteGroup* group = (RouteGroup *) edit_group_list.row(row).get_data ();

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if ((*i)->edit_group() == group) {
			unselect_strip_in_display (*(*i));
		}
	}
}

void
Editor::add_edit_group (RouteGroup* group)
{
	list<string> names;

	names.push_back ("*");
	names.push_back (group->name());

	edit_group_list.rows().push_back (names);
	edit_group_list.rows().back().set_data (group);
	edit_group_list.rows().back().select();

	group->FlagsChanged.connect (bind (mem_fun(*this, &Editor::group_flags_changed), group));
}

void
Editor::group_flags_changed (void* src, RouteGroup* group)
{
	if (src != this) {
		// select row
	}

	CList_Helpers::RowIterator ri = edit_group_list.rows().find_data (group);

	if (group->is_active()) {
		edit_group_list.cell (ri->get_row_num(),0).set_pixmap (check_pixmap, check_mask);
	} else {
		edit_group_list.cell (ri->get_row_num(),0).set_pixmap (empty_pixmap, empty_mask);
	}
}

