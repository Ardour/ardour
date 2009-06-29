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
#include "ardour/route_group.h"

#include "editor.h"
#include "keyboard.h"
#include "marker.h"
#include "time_axis_view.h"
#include "prompter.h"
#include "gui_thread.h"
#include "editor_group_tabs.h"
#include "route_group_dialog.h"
#include "route_time_axis.h"

#include "ardour/route.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

void
Editor::build_route_group_menu (RouteGroup* g)
{
	using namespace Gtk::Menu_Helpers;

	delete route_group_menu;

	Menu* new_from = new Menu;
	MenuList& f = new_from->items ();
	f.push_back (MenuElem (_("Selection..."), mem_fun (*this, &Editor::new_route_group_from_selection)));
	f.push_back (MenuElem (_("Record Enabled..."), mem_fun (*this, &Editor::new_route_group_from_rec_enabled)));
	f.push_back (MenuElem (_("Soloed..."), mem_fun (*this, &Editor::new_route_group_from_soloed)));

	route_group_menu = new Menu;
	route_group_menu->set_name ("ArdourContextMenu");
	MenuList& items = route_group_menu->items();

	items.push_back (MenuElem (_("New..."), mem_fun(*this, &Editor::new_route_group)));
	items.push_back (MenuElem (_("New From"), *new_from));
	if (g) {
		items.push_back (MenuElem (_("Edit..."), bind (mem_fun (*this, &Editor::edit_route_group), g)));
		items.push_back (MenuElem (_("Fit to Window"), bind (mem_fun (*this, &Editor::fit_route_group), g)));
		items.push_back (MenuElem (_("Subgroup"), bind (mem_fun (*this, &Editor::subgroup_route_group), g)));
	}
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Activate All"), mem_fun(*this, &Editor::activate_all_route_groups)));
	items.push_back (MenuElem (_("Disable All"), mem_fun(*this, &Editor::disable_all_route_groups)));
}

void
Editor::subgroup_route_group (RouteGroup* g)
{
	g->make_subgroup ();
}

void
Editor::unsubgroup_route_group (RouteGroup* g)
{
	g->destroy_subgroup ();
}

void
Editor::activate_all_route_groups ()
{
	session->foreach_route_group (bind (mem_fun (*this, &Editor::set_route_group_activation), true));
}

void
Editor::disable_all_route_groups ()
{
	session->foreach_route_group (bind (mem_fun (*this, &Editor::set_route_group_activation), false));
}

void
Editor::set_route_group_activation (RouteGroup* g, bool a)
{
	g->set_active (a, this);
}

void
Editor::new_route_group ()
{
	RouteGroup* g = new RouteGroup (*session, "", RouteGroup::Active, (RouteGroup::Property) (RouteGroup::Mute | RouteGroup::Solo | RouteGroup::Edit));

	RouteGroupDialog d (g, Gtk::Stock::NEW);
	int const r = d.do_run ();

	if (r == Gtk::RESPONSE_OK) {
		session->add_route_group (g);
	} else {
		delete g;
	}
}

void
Editor::new_route_group_from_selection ()
{
	RouteGroup* g = new RouteGroup (*session, "", RouteGroup::Active, (RouteGroup::Property) (RouteGroup::Mute | RouteGroup::Solo | RouteGroup::Edit | RouteGroup::Select));

	RouteGroupDialog d (g, Gtk::Stock::NEW);
	int const r = d.do_run ();

	if (r == Gtk::RESPONSE_OK) {
		session->add_route_group (g);

		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
			if (rtv) {
				rtv->route()->set_route_group (g, this);
			}
		}
		
	} else {
		delete g;
	}
}

void
Editor::new_route_group_from_rec_enabled ()
{
	RouteGroup* g = new RouteGroup (*session, "", RouteGroup::Active, (RouteGroup::Property) (RouteGroup::Mute | RouteGroup::Solo | RouteGroup::Edit | RouteGroup::RecEnable));

	RouteGroupDialog d (g, Gtk::Stock::NEW);
	int const r = d.do_run ();

	if (r == Gtk::RESPONSE_OK) {
		session->add_route_group (g);

		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
			if (rtv && rtv->route()->record_enabled()) {
				rtv->route()->set_route_group (g, this);
			}
		}
		
	} else {
		delete g;
	}
}

void
Editor::new_route_group_from_soloed ()
{
	RouteGroup* g = new RouteGroup (*session, "", RouteGroup::Active, (RouteGroup::Property) (RouteGroup::Mute | RouteGroup::Solo | RouteGroup::Edit));

	RouteGroupDialog d (g, Gtk::Stock::NEW);
	int const r = d.do_run ();

	if (r == Gtk::RESPONSE_OK) {
		session->add_route_group (g);

		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
			if (rtv && !rtv->route()->is_master() && rtv->route()->soloed()) {
				rtv->route()->set_route_group (g, this);
			}
		}
		
	} else {
		delete g;
	}
}

void
Editor::edit_route_group (RouteGroup* g)
{
	RouteGroupDialog d (g, Gtk::Stock::APPLY);
	d.do_run ();
}

void
Editor::remove_selected_route_group ()
{
	Glib::RefPtr<TreeSelection> selection = route_group_display.get_selection();
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
			session->remove_route_group (*rg);
		}
	}
}

void
Editor::route_group_list_button_clicked ()
{
	new_route_group ();
}

gint
Editor::route_group_list_button_press_event (GdkEventButton* ev)
{
	TreeModel::Path path;
	TreeIter iter;
        RouteGroup* group = 0;
	TreeViewColumn* column;
	int cellx;
	int celly;

	bool const p = route_group_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly);

	if (p) {
		iter = group_model->get_iter (path);
	}
	
	if (iter) {
		group = (*iter)[group_columns.routegroup];
	}

	if (Keyboard::is_context_menu_event (ev)) {
		build_route_group_menu (group);
		route_group_menu->popup (1, ev->time);
		return true;
	}

	if (!p) {
		return 1;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		if (Keyboard::is_edit_event (ev)) {
			if ((iter = group_model->get_iter (path))) {
				if ((group = (*iter)[group_columns.routegroup]) != 0) {
					// edit_route_group (group);
#ifdef GTKOSX
					route_group_display.queue_draw();
#endif
					return true;
				}
			}
			
		} 
		break;

	case 1:
		if ((iter = group_model->get_iter (path))) {
			bool gain = (*iter)[group_columns.gain];
			(*iter)[group_columns.gain] = !gain;
#ifdef GTKOSX
			route_group_display.queue_draw();
#endif
			return true;
		}
		break;

	case 2:
		if ((iter = group_model->get_iter (path))) {
			bool record = (*iter)[group_columns.record];
			(*iter)[group_columns.record] = !record;
#ifdef GTKOSX
			route_group_display.queue_draw();
#endif
			return true;
		}
		break;

	case 3:
		if ((iter = group_model->get_iter (path))) {
			bool mute = (*iter)[group_columns.mute];
			(*iter)[group_columns.mute] = !mute;
#ifdef GTKOSX
			route_group_display.queue_draw();
#endif
			return true;
		}
		break;

	case 4:
		if ((iter = group_model->get_iter (path))) {
			bool solo = (*iter)[group_columns.solo];
			(*iter)[group_columns.solo] = !solo;
#ifdef GTKOSX
			route_group_display.queue_draw();
#endif
			return true;
		}
		break;

	case 5:
		if ((iter = group_model->get_iter (path))) {
			bool select = (*iter)[group_columns.select];
			(*iter)[group_columns.select] = !select;
#ifdef GTKOSX
			route_group_display.queue_draw();
#endif
			return true;
		}
		break;

	case 6:
		if ((iter = group_model->get_iter (path))) {
			bool edits = (*iter)[group_columns.edits];
			(*iter)[group_columns.edits] = !edits;
#ifdef GTKOSX
			route_group_display.queue_draw();
#endif
			return true;
		}
		break;

	case 7:
		if ((iter = group_model->get_iter (path))) {
			bool visible = (*iter)[group_columns.is_visible];
			(*iter)[group_columns.is_visible] = !visible;
#ifdef GTKOSX
			route_group_display.queue_draw();
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
Editor::route_group_row_change (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter)
{
	RouteGroup* group;

	if (in_route_group_row_change) {
		return;
	}

	if ((group = (*iter)[group_columns.routegroup]) == 0) {
		return;
	}

	if ((*iter)[group_columns.is_visible]) {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				show_track_in_display (**j);
			}
		}
	} else {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				hide_track_in_display (**j);
			}
		}
	}

	if ((*iter)[group_columns.gain]) {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Gain, true);
			}
		}
	} else {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Gain, false);
			}
		}
	}

	if ((*iter)[group_columns.record]) {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::RecEnable, true);
			}
		}
	} else {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::RecEnable, false);
			}
		}
	}

	if ((*iter)[group_columns.mute]) {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Mute, true);
			}
		}
	} else {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Mute, false);
			}
		}
	}

	if ((*iter)[group_columns.solo]) {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Solo, true);
			}
		}
	} else {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Solo, false);
			}
		}
	}

	if ((*iter)[group_columns.select]) {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Select, true);
			}
		}
	} else {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Select, false);
			}
		}
	}

	if ((*iter)[group_columns.edits]) {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Edit, true);
			}
		}
	} else {
		for (TrackViewList::iterator j = track_views.begin(); j != track_views.end(); ++j) {
			if ((*j)->route_group() == group) {
				group->set_property (RouteGroup::Edit, false);
			}
		}
	}

	string name = (*iter)[group_columns.text];

	if (name != group->name()) {
		group->set_name (name);
	}
}

void
Editor::add_route_group (RouteGroup* group)
{
        ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::add_route_group), group));
	bool focus = false;

	TreeModel::Row row = *(group_model->append());
	
	row[group_columns.is_visible] = !group->is_hidden();
	row[group_columns.gain] = group->property(RouteGroup::Gain);
	row[group_columns.record] = group->property(RouteGroup::RecEnable);
	row[group_columns.mute] = group->property(RouteGroup::Mute);
	row[group_columns.solo] = group->property(RouteGroup::Solo);
	row[group_columns.select] = group->property(RouteGroup::Select);
	row[group_columns.edits] = group->property(RouteGroup::Edit);

	in_route_group_row_change = true;

	row[group_columns.routegroup] = group;

	if (!group->name().empty()) {
		row[group_columns.text] = group->name();
	} else {
		row[group_columns.text] = _("unnamed");
		focus = true;
	}

	group->FlagsChanged.connect (bind (mem_fun(*this, &Editor::group_flags_changed), group));

	if (focus) {  
		TreeViewColumn* col = route_group_display.get_column (0);
		CellRendererText* name_cell = dynamic_cast<CellRendererText*>(route_group_display.get_column_cell_renderer (0));
		route_group_display.set_cursor (group_model->get_path (row), *col, *name_cell, true);
	}

	in_route_group_row_change = false;

	_group_tabs->set_dirty ();
}

void
Editor::route_groups_changed ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &Editor::route_groups_changed));

	/* just rebuild the while thing */

	group_model->clear ();

	{
		TreeModel::Row row;
		row = *(group_model->append());
		row[group_columns.is_visible] = true;
		row[group_columns.text] = (_("-all-"));
		row[group_columns.routegroup] = 0;
	}

	session->foreach_route_group (mem_fun (*this, &Editor::add_route_group));
}

void
Editor::group_flags_changed (void* src, RouteGroup* group)
{
        ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::group_flags_changed), src, group));

	in_route_group_row_change = true;

        Gtk::TreeModel::Children children = group_model->children();

	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		if (group == (*iter)[group_columns.routegroup]) {
			(*iter)[group_columns.is_visible] = !group->is_hidden();
			(*iter)[group_columns.text] = group->name();
			(*iter)[group_columns.gain] = group->property(RouteGroup::Gain);
			(*iter)[group_columns.record] = group->property(RouteGroup::RecEnable);
			(*iter)[group_columns.mute] = group->property(RouteGroup::Mute);
			(*iter)[group_columns.solo] = group->property(RouteGroup::Solo);
			(*iter)[group_columns.select] = group->property(RouteGroup::Select);
			(*iter)[group_columns.edits] = group->property(RouteGroup::Edit);
		}
	}

	in_route_group_row_change = false;

	_group_tabs->set_dirty ();
}

void
Editor::route_group_name_edit (const Glib::ustring& path, const Glib::ustring& new_text)
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
