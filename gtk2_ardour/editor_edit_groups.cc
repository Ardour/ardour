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
Editor::build_edit_group_list_menu ()
{
	using namespace Gtk::Menu_Helpers;

	edit_group_list_menu = new Menu;
	edit_group_list_menu->set_name ("ArdourContextMenu");
	MenuList& items = edit_group_list_menu->items();

	items.push_back (MenuElem (_("Activate All"), mem_fun(*this, &Editor::activate_all_edit_groups)));
	items.push_back (MenuElem (_("Disable All"), mem_fun(*this, &Editor::disable_all_edit_groups)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Add group"), mem_fun(*this, &Editor::new_edit_group)));
	
}

void
Editor::activate_all_edit_groups ()
{
        Gtk::TreeModel::Children children = group_model->children();
	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
	        (*iter)[group_columns.is_active] = true;
	}
}

void
Editor::disable_all_edit_groups ()
{
        Gtk::TreeModel::Children children = group_model->children();
	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
	        (*iter)[group_columns.is_active] = false;
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
	case Gtk::RESPONSE_ACCEPT:
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
	if (Keyboard::is_context_menu_event (ev)) {
		if (edit_group_list_menu == 0) {
			build_edit_group_list_menu ();
		}
		edit_group_list_menu->popup (1, 0);
		return true;
	}


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
	case 2:
		if (Keyboard::is_edit_event (ev)) {
			if ((iter = group_model->get_iter (path))) {
				if ((group = (*iter)[group_columns.routegroup]) != 0) {
					// edit_route_group (group);
					return true;
				}
			}
			
		} 
		break;

	case 1:
		if ((iter = group_model->get_iter (path))) {
			bool visible = (*iter)[group_columns.is_visible];
			(*iter)[group_columns.is_visible] = !visible;
			return true;
		}
		break;

	case 0:
		if ((iter = group_model->get_iter (path))) {
			bool active = (*iter)[group_columns.is_active];
			(*iter)[group_columns.is_active] = !active;
			return true;
		}
		break;
		
	default:
		break;
	}
	
	return false;
 }

void 
Editor::edit_group_row_change (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter)
{
	RouteGroup* group;

	if ((group = (*iter)[group_columns.routegroup]) == 0) {
		return;
	}

	if ((*iter)[group_columns.is_visible]) {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->edit_group() == group) {
				show_track_in_display (**j);
			}
		}
	} else {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->edit_group() == group) {
				hide_track_in_display (**j);
			}
		}
	}

	bool active = (*iter)[group_columns.is_active];
	group->set_active (active, this);
}

void
Editor::add_edit_group (RouteGroup* group)
{
        ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::add_edit_group), group));

	TreeModel::Row row = *(group_model->append());
	row[group_columns.is_active] = group->is_active();
	row[group_columns.is_visible] = true;
	row[group_columns.text] = group->name();
	row[group_columns.routegroup] = group;

	group->FlagsChanged.connect (bind (mem_fun(*this, &Editor::group_flags_changed), group));
}

void
Editor::group_flags_changed (void* src, RouteGroup* group)
{
        ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::group_flags_changed), src, group));

        Gtk::TreeModel::Children children = group_model->children();
	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		if (group == (*iter)[group_columns.routegroup]) {
			(*iter)[group_columns.is_active] = group->is_active();
		}
	}
}


