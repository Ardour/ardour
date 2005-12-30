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
#include <gtkmm2ext/gtk_ui.h>
#include <ardour/route_group.h>

#include "editor.h"
#include "keyboard.h"
#include "marker.h"
#include "time_axis_view.h"
#include "prompter.h"
#include "gui_thread.h"

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
 
	/* XXX potential race with remove_track(), but the select operation
	   cannot be done with the track_lock held.
	*/

        Gtk::TreeModel::Children children = group_model->children();
	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
	        edit_group_display.get_selection()->select (iter);
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
	prompter.show_all ();

	switch (prompter.run ()) {
	case GTK_RESPONSE_ACCEPT:
	        prompter.get_result (result);
		if (result.length()) {
		  session->add_edit_group (result);
		}
		break;
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
        RouteGroup* group;
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	
	if (!edit_group_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	  
	case 1:

		if (Keyboard::is_edit_event (ev)) {
			// RouteGroup* group = (RouteGroup *) edit_group_display.row(row).get_data ();
			// edit_route_group (group);

			return stop_signal (edit_group_display, "button_press_event");

		} else {
			/* allow regular select to occur */
			return FALSE;
		}
		break;

	case 0:
		if ((iter = group_model->get_iter (path))) {
			/* path points to a valid node */
			
		        if ((group = (*iter)[group_columns.routegroup]) != 0) {
				group->set_active (!group->is_active (), this);
			}
		}
		break;
	}
      
	return stop_signal (edit_group_display, "button_press_event");
}

void
Editor::edit_group_selection_changed ()
{
	TreeModel::iterator i;
	TreeModel::Children rows = group_model->children();
	Glib::RefPtr<TreeSelection> selection = edit_group_display.get_selection();

	for (i = rows.begin(); i != rows.end(); ++i) {
		RouteGroup* group;

		group = (*i)[group_columns.routegroup];

		if (selection->is_selected (i)) {
		  for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
		    if ((*j)->edit_group() == group) {
		      select_strip_in_display (*j);
		    }
		  }
		} else {
		  for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
		    if ((*j)->edit_group() == group) {
		      unselect_strip_in_display (**j);
		    }
		  }
		}
	}
}

void
Editor::add_edit_group (RouteGroup* group)

{
        
        ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::add_edit_group), group));

	TreeModel::Row row = *(group_model->append());
	row[group_columns.is_active] = group->is_active();
	row[group_columns.text] = group->name();
	row[group_columns.routegroup] = group;

	group->FlagsChanged.connect (bind (mem_fun(*this, &Editor::group_flags_changed), group));
}

void
Editor::group_flags_changed (void* src, RouteGroup* group)
{
  /* GTK2FIX not needed in gtk2?

	if (src != this) {
		// select row
	}

	CList_Helpers::RowIterator ri = edit_group_display.rows().find_data (group);

	if (group->is_active()) {
		edit_group_display.cell (ri->get_row_num(),0).set_pixmap (check_pixmap, check_mask);
	} else {
		edit_group_display.cell (ri->get_row_num(),0).set_pixmap (empty_pixmap, empty_mask);
	}
  */
}


