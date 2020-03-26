/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <cstdlib>
#include <cmath>

#include "fix_carbon.h"

#include <gtkmm/stock.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/cell_renderer_color_selector.h"

#include "widgets/tooltips.h"

#include "ardour/route_group.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "ardour_ui.h"
#include "editor.h"
#include "editor_group_tabs.h"
#include "editor_route_groups.h"
#include "editor_routes.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "marker.h"
#include "route_group_dialog.h"
#include "route_time_axis.h"
#include "time_axis_view.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtk;
using Gtkmm2ext::Keyboard;

struct ColumnInfo {
	int         index;
	const char* label;
	const char* tooltip;
};

EditorRouteGroups::EditorRouteGroups (Editor* e)
	: EditorComponent (e)
	, _in_row_change (false)
	, _in_rebuild (false)
{
	_model = ListStore::create (_columns);
	_display.set_model (_model);

	Gtkmm2ext::CellRendererColorSelector* color_renderer = manage (new Gtkmm2ext::CellRendererColorSelector);
	TreeViewColumn* color_column = manage (new TreeViewColumn ("", *color_renderer));

	color_column->add_attribute (color_renderer->property_color(), _columns.gdkcolor);

	_display.append_column (*color_column);

	_display.append_column ("", _columns.text);
	_display.append_column ("", _columns.is_visible);
	_display.append_column ("", _columns.active_state);
	_display.append_column ("", _columns.gain);
	_display.append_column ("", _columns.gain_relative);
	_display.append_column ("", _columns.mute);
	_display.append_column ("", _columns.solo);
	_display.append_column ("", _columns.record);
	_display.append_column ("", _columns.monitoring);
	_display.append_column ("", _columns.select);
	_display.append_column ("", _columns.active_shared);

	TreeViewColumn* col;
	Gtk::Label* l;

	ColumnInfo ci[] = {
		{ 0,   _("Col"),            _("Group Tab Color") },
		{ 1,   _("Name"),           _("Name of Group") },
		{ 2,  S_("Visible|V"),      _("Group is visible?") },
		{ 3,   _("On"),             _("Group is enabled?") },
		{ 4,  S_("Group|G"),        _("Sharing Gain?") },
		{ 5,  S_("Relative|Rel"),   _("Relative Gain Changes?") },
		{ 6,  S_("Mute|M"),         _("Sharing Mute?") },
		{ 7,  S_("Solo|S"),         _("Sharing Solo?") },
		{ 8,   _("Rec"),            _("Sharing Record-enable Status?") },
		{ 9,  S_("Monitoring|Mon"), _("Sharing Monitoring Choice?") },
		{ 10, S_("Selection|Sel"),  _("Sharing Selected/Editing Status?") },
		{ 11, S_("Active|A"),       _("Sharing Active Status?") },
		{ -1, 0, 0 }
	};


	for (int i = 0; ci[i].index >= 0; ++i) {
		col = _display.get_column (ci[i].index);
		l = manage (new Label (ci[i].label));
		set_tooltip (*l, ci[i].tooltip);
		col->set_widget (*l);
		l->show ();

		col->set_data (X_("colnum"), GUINT_TO_POINTER(i));
		if (i == 1) {
			col->set_expand (true);
		} else {
			col->set_expand (false);
			col->set_alignment (ALIGN_CENTER);
		}
	}

	_display.set_headers_visible (true);

	color_dialog.get_colorsel()->set_has_opacity_control (false);
	color_dialog.get_colorsel()->set_has_palette (true);
	color_dialog.get_ok_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_ACCEPT));
	color_dialog.get_cancel_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_CANCEL));

	/* name is directly editable */

	CellRendererText* name_cell = dynamic_cast<CellRendererText*>(_display.get_column_cell_renderer (1));
	name_cell->property_editable() = true;
	name_cell->signal_edited().connect (sigc::mem_fun (*this, &EditorRouteGroups::name_edit));

	for (int i = 1; ci[i].index >= 0; ++i) {
		CellRendererToggle* active_cell = dynamic_cast <CellRendererToggle*> (_display.get_column_cell_renderer (i));

		if (active_cell) {
			active_cell->property_activatable() = true;
			active_cell->property_radio() = false;
		}
	}

	_model->signal_row_changed().connect (sigc::mem_fun (*this, &EditorRouteGroups::row_change));
	/* What signal would you guess was emitted when the rows of your treeview are reordered
	   by a drag and drop?  signal_rows_reordered?  That would be far too easy.
	   No, signal_row_deleted().
	 */
	_model->signal_row_deleted().connect (sigc::mem_fun (*this, &EditorRouteGroups::row_deleted));

	_display.set_name ("EditGroupList");
	_display.get_selection()->set_mode (SELECTION_SINGLE);
	_display.set_headers_visible (true);
	_display.set_reorderable (false);
	_display.set_rules_hint (true);

	_scroller.add (_display);
	_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	_display.signal_button_press_event().connect (sigc::mem_fun(*this, &EditorRouteGroups::button_press_event), false);

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

	add_button->signal_clicked().connect (sigc::hide_return (sigc::mem_fun (*this, &EditorRouteGroups::run_new_group_dialog)));
	remove_button->signal_clicked().connect (sigc::mem_fun (*this, &EditorRouteGroups::remove_selected));

	button_box->pack_start (*add_button);
	button_box->pack_start (*remove_button);

	_display_packer.pack_start (_scroller, true, true);
	_display_packer.pack_start (*button_box, false, false);
}

void
EditorRouteGroups::remove_selected ()
{
	Glib::RefPtr<TreeSelection> selection = _display.get_selection();
	TreeView::Selection::ListHandle_Path rows = selection->get_selected_rows ();

	if (rows.empty() || _session->deletion_in_progress()) {
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
	run_new_group_dialog ();
}

bool
EditorRouteGroups::button_press_event (GdkEventButton* ev)
{
	TreeModel::Path path;
	TreeIter iter;
	RouteGroup* group = 0;
	TreeViewColumn* column;
	int cellx;
	int celly;
	bool ret = false;
	Gdk::Color c;
	bool val;

	bool const p = _display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly);

	if (p) {
		iter = _model->get_iter (path);
	}

	if (iter) {
		group = (*iter)[_columns.routegroup];
	}

	if (Keyboard::is_context_menu_event (ev)) {
		_editor->_group_tabs->get_menu(group)->popup (1, ev->time);
		return true;
	}

	if (!p) {
		/* cancel selection */
		_display.get_selection()->unselect_all ();
		/* end any editing by grabbing focus */
		_display.grab_focus ();
		return true;
	}

	group = (*iter)[_columns.routegroup];

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		c =  (*iter)[_columns.gdkcolor];
		color_dialog.get_colorsel()->set_previous_color (c);
		color_dialog.get_colorsel()->set_current_color (c);

		switch (color_dialog.run()) {
			case RESPONSE_ACCEPT:
				c = color_dialog.get_colorsel()->get_current_color();
				GroupTabs::set_group_color (group, gdk_color_to_rgba (c));
				break;

			default:
				break;
		}

		color_dialog.hide ();
		ret = true;
		break;

	case 1:
		if (Keyboard::is_edit_event (ev) && group) {
			/* we'll be editing now ... */
			ret = true;
		}
		break;

	case 2:
		val = (*iter)[_columns.is_visible];
		/* note subtle logic inverse here: we set the new value with
		   "val", rather than !val, because we're using ::set_hidden()
		   not a (non-existent) ::set_visible() call.
		*/
		group->set_hidden (val, this);
		ret = true;
		break;


	case 3:
		val = (*iter)[_columns.active_state];
		group->set_active (!val, this);
		ret = true;
		break;

	case 4:
		val = (*iter)[_columns.gain];
		group->set_gain (!val);
		ret = true;
		break;

	case 5:
		val = (*iter)[_columns.gain_relative];
		group->set_relative (!val, this);
		ret = true;
		break;

	case 6:
		val = (*iter)[_columns.mute];
		group->set_mute (!val);
		ret = true;
		break;

	case 7:
		val = (*iter)[_columns.solo];
		group->set_solo (!val);
		ret = true;
		break;

	case 8:
		val = (*iter)[_columns.record];
		group->set_recenable (!val);
		ret = true;
		break;

	case 9:
		val = (*iter)[_columns.monitoring];
		group->set_monitoring (!val);
		ret = true;
		break;

	case 10:
		val = (*iter)[_columns.select];
		group->set_select (!val);
		ret = true;
		break;

	case 11:
		val = (*iter)[_columns.active_shared];
		group->set_route_active (!val);
		ret = true;
		break;

	default:
		break;
	}

	return ret;
}

void
EditorRouteGroups::row_change (const Gtk::TreeModel::Path&, const Gtk::TreeModel::iterator& iter)
{
	RouteGroup* group;

	if (_in_row_change) {
		return;
	}

	if ((group = (*iter)[_columns.routegroup]) == 0) {
		return;
	}

	PropertyList plist;
	plist.add (Properties::name, string ((*iter)[_columns.text]));

	bool val = (*iter)[_columns.gain];
	plist.add (Properties::group_gain, val);
	val = (*iter)[_columns.gain_relative];
	plist.add (Properties::group_relative, val);
	val = (*iter)[_columns.mute];
	plist.add (Properties::group_mute, val);
	val = (*iter)[_columns.solo];
	plist.add (Properties::group_solo, val);
	val = (*iter)[_columns.record];
	plist.add (Properties::group_recenable, val);
	val = (*iter)[_columns.monitoring];
	plist.add (Properties::group_monitoring, val);
	val = (*iter)[_columns.select];
	plist.add (Properties::group_select, val);
	val = (*iter)[_columns.active_shared];
	plist.add (Properties::group_route_active, val);

	val = (*iter)[_columns.active_state];
	plist.add (Properties::active, val);
	val = (*iter)[_columns.is_visible];
	plist.add (Properties::hidden, !val);

	group->apply_changes (plist);

	GroupTabs::set_group_color ((*iter)[_columns.routegroup], gdk_color_to_rgba ((*iter)[_columns.gdkcolor]));
}

void
EditorRouteGroups::add (RouteGroup* group)
{
	ENSURE_GUI_THREAD (*this, &EditorRouteGroups::add, group)
	bool focus = false;

	TreeModel::Row row = *(_model->append());

	row[_columns.gain] = group->is_gain ();
	row[_columns.gain_relative] = group->is_relative ();
	row[_columns.mute] = group->is_mute ();
	row[_columns.solo] = group->is_solo ();
	row[_columns.record] = group->is_recenable();
	row[_columns.monitoring] = group->is_monitoring();
	row[_columns.select] = group->is_select ();
	row[_columns.active_shared] = group->is_route_active ();
	row[_columns.active_state] = group->is_active ();
	row[_columns.is_visible] = !group->is_hidden();

	Gdk::Color c;
	set_color_from_rgba (c, GroupTabs::group_color (group));
	row[_columns.gdkcolor] = c;

	_in_row_change = true;

	row[_columns.routegroup] = group;

	if (!group->name().empty()) {
		row[_columns.text] = group->name();
	} else {
		row[_columns.text] = _("unnamed");
		focus = true;
	}

	group->PropertyChanged.connect (_property_changed_connections, MISSING_INVALIDATOR, boost::bind (&EditorRouteGroups::property_changed, this, group, _1), gui_context());

	if (focus) {
		TreeViewColumn* col = _display.get_column (0);
		CellRendererText* name_cell = dynamic_cast<CellRendererText*>(_display.get_column_cell_renderer (1));
		_display.set_cursor (_model->get_path (row), *col, *name_cell, true);
	}

	_in_row_change = false;

	_editor->_group_tabs->set_dirty ();
}

void
EditorRouteGroups::groups_changed ()
{
	ENSURE_GUI_THREAD (*this, &EditorRouteGroups::groups_changed);

	_in_rebuild = true;

	/* just rebuild the while thing */

	_model->clear ();

	if (_session) {
		_session->foreach_route_group (sigc::mem_fun (*this, &EditorRouteGroups::add));
	}

	_in_rebuild = false;
}

void
EditorRouteGroups::property_changed (RouteGroup* group, const PropertyChange&)
{
	assert(group);
	_in_row_change = true;

	Gtk::TreeModel::Children children = _model->children();

	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		if (group == (*iter)[_columns.routegroup]) {

			/* we could check the PropertyChange and only set
			 * appropriate fields. but the amount of saved by doing
			 * that is pretty minimal, and this is nice and simple.
			 */

			(*iter)[_columns.text] = group->name();
			(*iter)[_columns.gain] = group->is_gain ();
			(*iter)[_columns.gain_relative] = group->is_relative ();
			(*iter)[_columns.mute] = group->is_mute ();
			(*iter)[_columns.solo] = group->is_solo ();
			(*iter)[_columns.record] = group->is_recenable ();
			(*iter)[_columns.monitoring] = group->is_monitoring ();
			(*iter)[_columns.select] = group->is_select ();
			(*iter)[_columns.active_shared] = group->is_route_active ();
			(*iter)[_columns.active_state] = group->is_active ();
			(*iter)[_columns.is_visible] = !group->is_hidden();

			Gdk::Color c;
			set_color_from_rgba (c, GroupTabs::group_color (group));
			(*iter)[_columns.gdkcolor] = c;

			break;
		}
	}

	_in_row_change = false;

	for (TrackViewList::const_iterator i = _editor->get_track_views().begin(); i != _editor->get_track_views().end(); ++i) {
		if ((*i)->route_group() == group) {
			if (group->is_hidden ()) {
				_editor->hide_track_in_display (*i);
			} else {
				_editor->_routes->show_track_in_display (**i);
			}
		}
	}
}

void
EditorRouteGroups::name_edit (const std::string& path, const std::string& new_text)
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
EditorRouteGroups::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {

		_session->route_group_added.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&EditorRouteGroups::add, this, _1), gui_context());
		_session->route_group_removed.connect (
			_session_connections, MISSING_INVALIDATOR, boost::bind (&EditorRouteGroups::groups_changed, this), gui_context()
			);
		_session->route_groups_reordered.connect (
			_session_connections, MISSING_INVALIDATOR, boost::bind (&EditorRouteGroups::groups_changed, this), gui_context()
			);
	}

	PBD::PropertyChange pc;
	pc.add (Properties::group_select);
	pc.add (Properties::active);

	groups_changed ();
}

void
EditorRouteGroups::run_new_group_dialog ()
{
	return _editor->_group_tabs->run_new_group_dialog (0, false);
}

/** Called when a model row is deleted, but also when the model is
 *  reordered by a user drag-and-drop; the latter is what we are
 *  interested in here.
 */
void
EditorRouteGroups::row_deleted (Gtk::TreeModel::Path const &)
{
	if (_in_rebuild || !_session || _session->deletion_in_progress()) {
		/* We need to ignore this in cases where we're not doing a drag-and-drop
		   re-order.
		*/
		return;
	}

	/* Re-write the session's route group list so that the new order is preserved */

	list<RouteGroup*> new_list;

	Gtk::TreeModel::Children children = _model->children();
	for (Gtk::TreeModel::Children::iterator i = children.begin(); i != children.end(); ++i) {
		new_list.push_back ((*i)[_columns.routegroup]);
	}

	_session->reorder_route_groups (new_list);
}


