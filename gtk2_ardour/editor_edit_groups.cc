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
using namespace PBD;
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
	items.push_back (MenuElem (_("Show All"), mem_fun(*this, &Editor::show_all_edit_groups)));
	items.push_back (MenuElem (_("Hide All"), mem_fun(*this, &Editor::hide_all_edit_groups)));
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
Editor::show_all_edit_groups ()
{
        Gtk::TreeModel::Children children = group_model->children();
	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
	        (*iter)[group_columns.is_visible] = true;
	}
}

void
Editor::hide_all_edit_groups ()
{
        Gtk::TreeModel::Children children = group_model->children();
	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
	        (*iter)[group_columns.is_visible] = false;
	}
}

void
Editor::new_edit_group ()
{
	session->add_edit_group ("");
}

void
Editor::remove_selected_edit_group ()
{
	Glib::RefPtr<TreeSelection> selection = edit_group_display.get_selection();
	TreeView::Selection::ListHandle_Path rows = selection->get_selected_rows ();

	if (rows.empty()) {
		return;
	}

	TreeView::Selection::ListHandle_Path::iterator i = rows.begin();
	TreeIter iter;
	
	/* selection mode is single, so rows.begin() is it */

	if ((iter = group_model->get_iter (*i))) {

		RouteGroup* rg = (*iter)[group_columns.routegroup];

		if (rg) {
			session->remove_edit_group (*rg);
		}
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
		edit_group_list_menu->popup (1, ev->time);
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
	case 0:
		if (Keyboard::is_edit_event (ev)) {
			if ((iter = group_model->get_iter (path))) {
				if ((group = (*iter)[group_columns.routegroup]) != 0) {
					// edit_route_group (group);
#ifdef GTKOSX
					edit_group_display.queue_draw();
#endif
					return true;
				}
			}
			
		} 
		break;

	case 1:
		if ((iter = group_model->get_iter (path))) {
			bool active = (*iter)[group_columns.is_active];
			(*iter)[group_columns.is_active] = !active;
#ifdef GTKOSX
			edit_group_display.queue_draw();
#endif
			return true;
		}
		break;
		
	case 2:
		if ((iter = group_model->get_iter (path))) {
			bool visible = (*iter)[group_columns.is_visible];
			(*iter)[group_columns.is_visible] = !visible;
#ifdef GTKOSX
			edit_group_display.queue_draw();
#endif
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

	if (in_edit_group_row_change) {
		return;
	}

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


	string name = (*iter)[group_columns.text];

	if (name != group->name()) {
		group->set_name (name);
	}
}

void
Editor::add_edit_group (RouteGroup* group)
{
        ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::add_edit_group), group));
	bool focus = false;

	TreeModel::Row row = *(group_model->append());
	row[group_columns.is_active] = group->is_active();
	row[group_columns.is_visible] = false;

	in_edit_group_row_change = true;

	row[group_columns.routegroup] = group;

	if (!group->name().empty()) {
		row[group_columns.text] = group->name();
	} else {
		row[group_columns.text] = _("unnamed");
		focus = true;
	}

	group->FlagsChanged.connect (bind (mem_fun(*this, &Editor::group_flags_changed), group));

	if (focus) {
		TreeViewColumn* col = edit_group_display.get_column (0);
		CellRendererText* name_cell = dynamic_cast<CellRendererText*>(edit_group_display.get_column_cell_renderer (0));
		edit_group_display.set_cursor (group_model->get_path (row), *col, *name_cell, true);
	}

	in_edit_group_row_change = false;
}

void
Editor::edit_groups_changed ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &Editor::edit_groups_changed));

	/* just rebuild the while thing */

	group_model->clear ();

	session->foreach_edit_group (mem_fun (*this, &Editor::add_edit_group));
}

void
Editor::group_flags_changed (void* src, RouteGroup* group)
{
        ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::group_flags_changed), src, group));

	in_edit_group_row_change = true;

        Gtk::TreeModel::Children children = group_model->children();
	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		if (group == (*iter)[group_columns.routegroup]) {
			(*iter)[group_columns.is_active] = group->is_active();
			(*iter)[group_columns.is_visible] = !group->is_hidden();
			(*iter)[group_columns.text] = group->name();
		}
	}

	in_edit_group_row_change = false;
}

void
Editor::edit_group_name_edit (const Glib::ustring& path, const Glib::ustring& new_text)
{
	RouteGroup* group;
	TreeIter iter;
	
	if ((iter = group_model->get_iter (path))) {
	
		if ((group = (*iter)[group_columns.routegroup]) == 0) {
			return;
		}
		
		if (new_text != group->name()) {
			group->set_name (new_text);
		}
	}
}
