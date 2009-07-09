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
#include "editor_routes.h"
#include "editor_route_groups.h"

#include "ardour/route.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

EditorRouteGroups::EditorRouteGroups (Editor* e)
	: EditorComponent (e),
	  _menu (0),
	  _in_row_change (false)

{
	_model = ListStore::create (_columns);
	_display.set_model (_model);

	_display.append_column (_("Name"), _columns.text);

	_display.append_column (_("G"), _columns.gain);
	_display.append_column (_("R"), _columns.record);
	_display.append_column (_("M"), _columns.mute);
	_display.append_column (_("S"), _columns.solo);
	_display.append_column (_("Sel"), _columns.select);
	_display.append_column (_("E"), _columns.edits);

	_display.append_column (_("Show"), _columns.is_visible);

	_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	_display.get_column (2)->set_data (X_("colnum"), GUINT_TO_POINTER(2));
	_display.get_column (3)->set_data (X_("colnum"), GUINT_TO_POINTER(3));
	_display.get_column (4)->set_data (X_("colnum"), GUINT_TO_POINTER(4));
	_display.get_column (5)->set_data (X_("colnum"), GUINT_TO_POINTER(5));
	_display.get_column (6)->set_data (X_("colnum"), GUINT_TO_POINTER(6));
	_display.get_column (7)->set_data (X_("colnum"), GUINT_TO_POINTER(7));

	_display.get_column (0)->set_expand (true);
	_display.get_column (1)->set_expand (false);
	_display.get_column (2)->set_expand (false);
	_display.get_column (3)->set_expand (false);
	_display.get_column (4)->set_expand (false);
	_display.get_column (5)->set_expand (false);
	_display.get_column (6)->set_expand (false);
	_display.get_column (7)->set_expand (false);

	_display.set_headers_visible (true);

	/* name is directly editable */

	CellRendererText* name_cell = dynamic_cast<CellRendererText*>(_display.get_column_cell_renderer (0));
	name_cell->property_editable() = true;
	name_cell->signal_edited().connect (mem_fun (*this, &EditorRouteGroups::name_edit));

	/* use checkbox for the active + visible columns */

	CellRendererToggle* active_cell = dynamic_cast<CellRendererToggle*>(_display.get_column_cell_renderer (1));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	active_cell = dynamic_cast<CellRendererToggle*>(_display.get_column_cell_renderer (2));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	active_cell = dynamic_cast<CellRendererToggle*>(_display.get_column_cell_renderer (3));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	active_cell = dynamic_cast<CellRendererToggle*>(_display.get_column_cell_renderer (4));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	active_cell = dynamic_cast<CellRendererToggle*>(_display.get_column_cell_renderer (5));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	active_cell = dynamic_cast<CellRendererToggle*>(_display.get_column_cell_renderer (6));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	active_cell = dynamic_cast<CellRendererToggle*>(_display.get_column_cell_renderer (7));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	_model->signal_row_changed().connect (mem_fun (*this, &EditorRouteGroups::row_change));

	_display.set_name ("EditGroupList");
	_display.get_selection()->set_mode (SELECTION_SINGLE);
	_display.set_headers_visible (true);
	_display.set_reorderable (false);
	_display.set_rules_hint (true);
	_display.set_size_request (75, -1);

	_scroller.add (_display);
	_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	_display.signal_button_press_event().connect (mem_fun(*this, &EditorRouteGroups::button_press_event), false);

	_display_packer = new VBox;
	HBox* button_box = manage (new HBox());
	button_box->set_homogeneous (true);

	Button* add_button = manage (new Button ());
	Button* remove_button = manage (new Button ());

	Widget* w;

	w = manage (new Image (Stock::ADD, ICON_SIZE_BUTTON));
	w->show();
	add_button->add (*w);

	w = manage (new Image (Stock::REMOVE, ICON_SIZE_BUTTON));
	w->show();
	remove_button->add (*w);

	add_button->signal_clicked().connect (mem_fun (*this, &EditorRouteGroups::new_route_group));
	remove_button->signal_clicked().connect (mem_fun (*this, &EditorRouteGroups::remove_selected));
	
	button_box->pack_start (*add_button);
	button_box->pack_start (*remove_button);

	_display_packer->pack_start (_scroller, true, true);
	_display_packer->pack_start (*button_box, false, false);
}
	

Gtk::Menu*
EditorRouteGroups::menu (RouteGroup* g)
{
	using namespace Gtk::Menu_Helpers;

	delete _menu;

	Menu* new_from = new Menu;
	MenuList& f = new_from->items ();
	f.push_back (MenuElem (_("Selection..."), mem_fun (*this, &EditorRouteGroups::new_from_selection)));
	f.push_back (MenuElem (_("Record Enabled..."), mem_fun (*this, &EditorRouteGroups::new_from_rec_enabled)));
	f.push_back (MenuElem (_("Soloed..."), mem_fun (*this, &EditorRouteGroups::new_from_soloed)));

	_menu = new Menu;
	_menu->set_name ("ArdourContextMenu");
	MenuList& items = _menu->items();

	items.push_back (MenuElem (_("New..."), mem_fun(*this, &EditorRouteGroups::new_route_group)));
	items.push_back (MenuElem (_("New From"), *new_from));
	if (g) {
		items.push_back (MenuElem (_("Edit..."), bind (mem_fun (*this, &EditorRouteGroups::edit), g)));
		items.push_back (MenuElem (_("Fit to Window"), bind (mem_fun (*_editor, &Editor::fit_route_group), g)));
		items.push_back (MenuElem (_("Subgroup"), bind (mem_fun (*this, &EditorRouteGroups::subgroup), g)));
		items.push_back (MenuElem (_("Collect"), bind (mem_fun (*this, &EditorRouteGroups::collect), g)));
	}
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Activate All"), mem_fun(*this, &EditorRouteGroups::activate_all)));
	items.push_back (MenuElem (_("Disable All"), mem_fun(*this, &EditorRouteGroups::disable_all)));

	return _menu;
}

void
EditorRouteGroups::subgroup (RouteGroup* g)
{
	g->make_subgroup ();
}

void
EditorRouteGroups::unsubgroup (RouteGroup* g)
{
	g->destroy_subgroup ();
}

void
EditorRouteGroups::activate_all ()
{
	_session->foreach_route_group (
		bind (mem_fun (*this, &EditorRouteGroups::set_activation), true)
		);
}

void
EditorRouteGroups::disable_all ()
{
	_session->foreach_route_group (
		bind (mem_fun (*this, &EditorRouteGroups::set_activation), false)
		);
}

void
EditorRouteGroups::set_activation (RouteGroup* g, bool a)
{
	g->set_active (a, this);
}

void
EditorRouteGroups::new_route_group ()
{
	RouteGroup* g = new RouteGroup (
		*_session,
		"",
		RouteGroup::Active,
		(RouteGroup::Property) (RouteGroup::Mute | RouteGroup::Solo | RouteGroup::Edit)
		);

	RouteGroupDialog d (g, Gtk::Stock::NEW);
	int const r = d.do_run ();

	if (r == Gtk::RESPONSE_OK) {
		_session->add_route_group (g);
	} else {
		delete g;
	}
}

void
EditorRouteGroups::new_from_selection ()
{
	RouteGroup* g = new RouteGroup (
		*_session,
		"",
		RouteGroup::Active,
		(RouteGroup::Property) (RouteGroup::Mute | RouteGroup::Solo | RouteGroup::Edit | RouteGroup::Select)
		);

	RouteGroupDialog d (g, Gtk::Stock::NEW);
	int const r = d.do_run ();

	if (r == Gtk::RESPONSE_OK) {
		_session->add_route_group (g);

		for (TrackSelection::iterator i = _editor->get_selection().tracks.begin(); i != _editor->get_selection().tracks.end(); ++i) {
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
EditorRouteGroups::new_from_rec_enabled ()
{
	RouteGroup* g = new RouteGroup (
		*_session,
		"",
		RouteGroup::Active,
		(RouteGroup::Property) (RouteGroup::Mute | RouteGroup::Solo | RouteGroup::Edit | RouteGroup::RecEnable)
		);

	RouteGroupDialog d (g, Gtk::Stock::NEW);
	int const r = d.do_run ();

	if (r == Gtk::RESPONSE_OK) {
		_session->add_route_group (g);

		for (Editor::TrackViewList::const_iterator i = _editor->get_track_views().begin(); i != _editor->get_track_views().end(); ++i) {
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
EditorRouteGroups::new_from_soloed ()
{
	RouteGroup* g = new RouteGroup (
		*_session,
		"",
		RouteGroup::Active,
		(RouteGroup::Property) (RouteGroup::Mute | RouteGroup::Solo | RouteGroup::Edit)
		);

	RouteGroupDialog d (g, Gtk::Stock::NEW);
	int const r = d.do_run ();

	if (r == Gtk::RESPONSE_OK) {
		_session->add_route_group (g);

		for (Editor::TrackViewList::const_iterator i = _editor->get_track_views().begin(); i != _editor->get_track_views().end(); ++i) {
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
EditorRouteGroups::edit (RouteGroup* g)
{
	RouteGroupDialog d (g, Gtk::Stock::APPLY);
	d.do_run ();
}

void
EditorRouteGroups::remove_selected ()
{
	Glib::RefPtr<TreeSelection> selection = _display.get_selection();
	TreeView::Selection::ListHandle_Path rows = selection->get_selected_rows ();

	if (rows.empty()) {
		return;
	}

	TreeView::Selection::ListHandle_Path::iterator i = rows.begin();
	TreeIter iter;
	
	/* selection mode is single, so rows.begin() is it */

	if ((iter = _model->get_iter (*i))) {

		RouteGroup* rg = (*iter)[_columns.routegroup];

		if (rg) {
			_session->remove_route_group (*rg);
		}
	}
}

void
EditorRouteGroups::button_clicked ()
{
	new_route_group ();
}

gint
EditorRouteGroups::button_press_event (GdkEventButton* ev)
{
	TreeModel::Path path;
	TreeIter iter;
        RouteGroup* group = 0;
	TreeViewColumn* column;
	int cellx;
	int celly;

	bool const p = _display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly);

	if (p) {
		iter = _model->get_iter (path);
	}
	
	if (iter) {
		group = (*iter)[_columns.routegroup];
	}

	if (Keyboard::is_context_menu_event (ev)) {
		menu(group)->popup (1, ev->time);
		return true;
	}

	if (!p) {
		return 1;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		if (Keyboard::is_edit_event (ev)) {
			if ((iter = _model->get_iter (path))) {
				if ((group = (*iter)[_columns.routegroup]) != 0) {
					// edit_route_group (group);
#ifdef GTKOSX
					_display.queue_draw();
#endif
					return true;
				}
			}
			
		} 
		break;

	case 1:
		if ((iter = _model->get_iter (path))) {
			bool gain = (*iter)[_columns.gain];
			(*iter)[_columns.gain] = !gain;
#ifdef GTKOSX
			_display.queue_draw();
#endif
			return true;
		}
		break;

	case 2:
		if ((iter = _model->get_iter (path))) {
			bool record = (*iter)[_columns.record];
			(*iter)[_columns.record] = !record;
#ifdef GTKOSX
			_display.queue_draw();
#endif
			return true;
		}
		break;

	case 3:
		if ((iter = _model->get_iter (path))) {
			bool mute = (*iter)[_columns.mute];
			(*iter)[_columns.mute] = !mute;
#ifdef GTKOSX
			_display.queue_draw();
#endif
			return true;
		}
		break;

	case 4:
		if ((iter = _model->get_iter (path))) {
			bool solo = (*iter)[_columns.solo];
			(*iter)[_columns.solo] = !solo;
#ifdef GTKOSX
			_display.queue_draw();
#endif
			return true;
		}
		break;

	case 5:
		if ((iter = _model->get_iter (path))) {
			bool select = (*iter)[_columns.select];
			(*iter)[_columns.select] = !select;
#ifdef GTKOSX
			_display.queue_draw();
#endif
			return true;
		}
		break;

	case 6:
		if ((iter = _model->get_iter (path))) {
			bool edits = (*iter)[_columns.edits];
			(*iter)[_columns.edits] = !edits;
#ifdef GTKOSX
			_display.queue_draw();
#endif
			return true;
		}
		break;

	case 7:
		if ((iter = _model->get_iter (path))) {
			bool visible = (*iter)[_columns.is_visible];
			(*iter)[_columns.is_visible] = !visible;
#ifdef GTKOSX
			_display.queue_draw();
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
EditorRouteGroups::row_change (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter)
{
	RouteGroup* group;

	if (_in_row_change) {
		return;
	}

	if ((group = (*iter)[_columns.routegroup]) == 0) {
		return;
	}

	if ((*iter)[_columns.is_visible]) {
		for (Editor::TrackViewList::const_iterator j = _editor->get_track_views().begin(); j != _editor->get_track_views().end(); ++j) {
			if ((*j)->route_group() == group) {
				_editor->_routes->show_track_in_display (**j);
			}
		}
	} else {
		for (Editor::TrackViewList::const_iterator j = _editor->get_track_views().begin(); j != _editor->get_track_views().end(); ++j) {
			if ((*j)->route_group() == group) {
				_editor->hide_track_in_display (**j);
			}
		}
	}

	group->set_property (RouteGroup::Gain, (*iter)[_columns.gain]);
	group->set_property (RouteGroup::RecEnable, (*iter)[_columns.record]);
	group->set_property (RouteGroup::Mute, (*iter)[_columns.mute]);
	group->set_property (RouteGroup::Solo, (*iter)[_columns.solo]);
	group->set_property (RouteGroup::Select, (*iter)[_columns.select]);
	group->set_property (RouteGroup::Edit, (*iter)[_columns.edits]);

	string name = (*iter)[_columns.text];

	if (name != group->name()) {
		group->set_name (name);
	}
}

void
EditorRouteGroups::add (RouteGroup* group)
{
        ENSURE_GUI_THREAD (bind (mem_fun(*this, &EditorRouteGroups::add), group));
	bool focus = false;

	TreeModel::Row row = *(_model->append());
	
	row[_columns.is_visible] = !group->is_hidden();
	row[_columns.gain] = group->property(RouteGroup::Gain);
	row[_columns.record] = group->property(RouteGroup::RecEnable);
	row[_columns.mute] = group->property(RouteGroup::Mute);
	row[_columns.solo] = group->property(RouteGroup::Solo);
	row[_columns.select] = group->property(RouteGroup::Select);
	row[_columns.edits] = group->property(RouteGroup::Edit);

	_in_row_change = true;

	row[_columns.routegroup] = group;

	if (!group->name().empty()) {
		row[_columns.text] = group->name();
	} else {
		row[_columns.text] = _("unnamed");
		focus = true;
	}

	group->FlagsChanged.connect (bind (mem_fun (*this, &EditorRouteGroups::flags_changed), group));

	if (focus) {  
		TreeViewColumn* col = _display.get_column (0);
		CellRendererText* name_cell = dynamic_cast<CellRendererText*>(_display.get_column_cell_renderer (0));
		_display.set_cursor (_model->get_path (row), *col, *name_cell, true);
	}

	_in_row_change = false;

	_editor->_group_tabs->set_dirty ();
}

void
EditorRouteGroups::groups_changed ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &EditorRouteGroups::groups_changed));

	/* just rebuild the while thing */

	_model->clear ();

	{
		TreeModel::Row row;
		row = *(_model->append());
		row[_columns.is_visible] = true;
		row[_columns.text] = (_("-all-"));
		row[_columns.routegroup] = 0;
	}

	_session->foreach_route_group (mem_fun (*this, &EditorRouteGroups::add));
}

void
EditorRouteGroups::flags_changed (void* src, RouteGroup* group)
{
        ENSURE_GUI_THREAD (bind (mem_fun(*this, &EditorRouteGroups::flags_changed), src, group));

	_in_row_change = true;

        Gtk::TreeModel::Children children = _model->children();

	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		if (group == (*iter)[_columns.routegroup]) {
			(*iter)[_columns.is_visible] = !group->is_hidden();
			(*iter)[_columns.text] = group->name();
			(*iter)[_columns.gain] = group->property(RouteGroup::Gain);
			(*iter)[_columns.record] = group->property(RouteGroup::RecEnable);
			(*iter)[_columns.mute] = group->property(RouteGroup::Mute);
			(*iter)[_columns.solo] = group->property(RouteGroup::Solo);
			(*iter)[_columns.select] = group->property(RouteGroup::Select);
			(*iter)[_columns.edits] = group->property(RouteGroup::Edit);
		}
	}

	_in_row_change = false;

	_editor->_group_tabs->set_dirty ();
}

void
EditorRouteGroups::name_edit (const Glib::ustring& path, const Glib::ustring& new_text)
{
	RouteGroup* group;
	TreeIter iter;
	
	if ((iter = _model->get_iter (path))) {
	
		if ((group = (*iter)[_columns.routegroup]) == 0) {
			return;
		}
		
		if (new_text != group->name()) {
			group->set_name (new_text);
		}
	}
}

void
EditorRouteGroups::clear ()
{
	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->clear ();
	_display.set_model (_model);
}

void
EditorRouteGroups::connect_to_session (Session* s)
{
	EditorComponent::connect_to_session (s);

	_session_connections.push_back (_session->route_group_added.connect (mem_fun (*this, &EditorRouteGroups::add)));
	_session_connections.push_back (_session->route_group_removed.connect (mem_fun (*this, &EditorRouteGroups::groups_changed)));

	groups_changed ();
}

struct CollectSorter {
	bool operator () (Route* a, Route* b) {
		return a->order_key (N_ ("editor")) < b->order_key (N_ ("editor"));
	}
};

/** Collect all members of a RouteGroup so that they are together in the Editor.
 *  @param g Group to collect.
 */
void
EditorRouteGroups::collect (RouteGroup* g)
{
	list<Route*> routes = g->route_list ();
	routes.sort (CollectSorter ());
	int const N = routes.size ();

	list<Route*>::iterator i = routes.begin ();
	Editor::TrackViewList::const_iterator j = _editor->get_track_views().begin();

	int diff = 0;
	int coll = -1;
	while (i != routes.end() && j != _editor->get_track_views().end()) {

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*j);
		if (rtv) {

			boost::shared_ptr<Route> r = rtv->route ();
			int const k = r->order_key (N_ ("editor"));
			
			if (*i == r.get()) {

				if (coll == -1) {
					coll = k;
					diff = N - 1;
				} else {
					--diff;
				}
				
				r->set_order_key (N_ ("editor"), coll);
				
				++coll;
				++i;
				
			} else {
				
				r->set_order_key (N_ ("editor"), k + diff);
				
			}
		}
			
		++j;
	}

	_editor->_routes->sync_order_keys (N_ ("editor"));
}
