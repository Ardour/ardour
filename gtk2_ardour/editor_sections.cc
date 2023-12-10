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

#include <glibmm.h>

#include "ardour/session.h"
#include "pbd/unwind.h"

#include "gtkmm2ext/keyboard.h"

#include "ardour_ui.h"
#include "context_menu_helper.h"
#include "editor_sections.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "main_clock.h"
#include "public_editor.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace ARDOUR;

EditorSections::EditorSections ()
	: _no_redisplay (false)
{
	_model = ListStore::create (_columns);
	_view.set_model (_model);

	Gtk::TreeViewColumn* c = manage (new Gtk::TreeViewColumn (_("Name"), _columns.name));
	_view.append_column (*c);
	c->set_resizable (true);
	c->set_data ("mouse-edits-require-mod1", (gpointer)0x1);

	CellRendererText* section_name_cell     = dynamic_cast<CellRendererText*> (c->get_first_cell ());
	section_name_cell->property_editable () = true;
	section_name_cell->signal_edited ().connect (sigc::mem_fun (*this, &EditorSections::name_edited));

	_view.append_column (_("Start"), _columns.s_start);
	_view.append_column (_("End"), _columns.s_end);
	_view.set_enable_search (false);
	_view.set_headers_visible (true);
	_view.get_selection ()->set_mode (Gtk::SELECTION_SINGLE);

	_scroller.add (_view);
	_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	_view.signal_key_press_event ().connect (sigc::mem_fun (*this, &EditorSections::key_press), false);
	_view.signal_button_press_event ().connect (sigc::mem_fun (*this, &EditorSections::button_press), false);
	_view.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &EditorSections::selection_changed));

	/* DnD source */
	std::vector<TargetEntry> dnd;
	dnd.push_back (TargetEntry ("x-ardour/section", Gtk::TARGET_SAME_APP));
	_view.drag_source_set (dnd, Gdk::MODIFIER_MASK, Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
	_view.signal_drag_data_get ().connect (sigc::mem_fun (*this, &EditorSections::drag_data_get));

	/* DnD target */
	_view.drag_dest_set (dnd, DEST_DEFAULT_ALL, Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
	_view.signal_drag_begin ().connect (sigc::mem_fun (*this, &EditorSections::drag_begin));
	_view.signal_drag_motion ().connect (sigc::mem_fun (*this, &EditorSections::drag_motion));
	_view.signal_drag_leave ().connect (sigc::mem_fun (*this, &EditorSections::drag_leave));
	_view.signal_drag_data_received ().connect (sigc::mem_fun (*this, &EditorSections::drag_data_received));

	/* Allow to scroll using key up/down */
	_view.signal_enter_notify_event ().connect (sigc::mem_fun (*this, &EditorSections::enter_notify), false);
	_view.signal_leave_notify_event ().connect (sigc::mem_fun (*this, &EditorSections::leave_notify), false);

	ARDOUR_UI::instance ()->primary_clock->mode_changed.connect (sigc::mem_fun (*this, &EditorSections::clock_format_changed));

	_selection_change = PublicEditor::instance ().get_selection ().TimeChanged.connect (sigc::mem_fun (*this, &EditorSections::update_time_selection));
}

void
EditorSections::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {
		_session->locations ()->added.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::location_changed, this, _1), gui_context ());
		_session->locations ()->removed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::location_changed, this, _1), gui_context ());
		_session->locations ()->changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::queue_redisplay, this), gui_context ());

		Location::start_changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::location_changed, this, _1), gui_context ());
		Location::end_changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::location_changed, this, _1), gui_context ());
		Location::flags_changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::queue_redisplay, this), gui_context ());
		Location::name_changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::location_changed, this, _1), gui_context ());
	}

	redisplay ();
}

void
EditorSections::select (ARDOUR::Location* l)
{
	LocationRowMap::iterator map_it = _location_row_map.find (l);
	if (map_it != _location_row_map.end ()) {
		_view.get_selection ()->select (*map_it->second);
	}
}

void
EditorSections::location_changed (ARDOUR::Location* l)
{
	if (l->is_section ()) {
		queue_redisplay ();
	}
}

void
EditorSections::queue_redisplay ()
{
	if (!_redisplay_connection.connected ()) {
		_redisplay_connection = Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorSections::idle_redisplay), Glib::PRIORITY_HIGH_IDLE+10);
	}
}

bool
EditorSections::idle_redisplay ()
{
	redisplay ();
	return false;
}

void
EditorSections::redisplay ()
{
	if (_no_redisplay) {
		return;
	}
	_view.set_model (Glib::RefPtr<ListStore> ());
	_model->clear ();
	_location_row_map.clear ();

	if (_session == 0) {
		return;
	}

	timepos_t start;
	timepos_t end;
	std::vector<Locations::LocationPair> locs;

	Locations* loc = _session->locations ();
	Location*  l   = NULL;

	do {
		l = loc->next_section_iter (l, start, end, locs);
		if (l) {
			TreeModel::Row newrow     = *(_model->append ());
			newrow[_columns.name]     = l->name ();
			newrow[_columns.location] = l;
			newrow[_columns.start]    = start;
			newrow[_columns.end]      = end;

			_location_row_map.insert (pair<ARDOUR::Location*, Gtk::TreeModel::iterator> (l, newrow));
		}
	} while (l);

	clock_format_changed ();

	_view.set_model (_model);
}

void
EditorSections::clock_format_changed ()
{
	if (!_session) {
		return;
	}

	TreeModel::Children rows = _model->children ();
	for (auto const& r : rows) {
		char buf[16];
		ARDOUR_UI_UTILS::format_position (_session, r[_columns.start], buf, sizeof (buf));
		r[_columns.s_start] = buf;
		ARDOUR_UI_UTILS::format_position (_session, r[_columns.end], buf, sizeof (buf));
		r[_columns.s_end] = buf;
	}
}

bool
EditorSections::scroll_row_timeout ()
{
	int              y;
	Gdk::Rectangle   visible_rect;
	Gtk::Adjustment* adj = _scroller.get_vadjustment ();

	gdk_window_get_pointer (_view.get_window ()->gobj (), NULL, &y, NULL);
	_view.get_visible_rect (visible_rect);

	y += adj->get_value ();

	int offset = y - (visible_rect.get_y () + 30);
	if (offset > 0) {
		offset = y - (visible_rect.get_y () + visible_rect.get_height () - 30);
		if (offset < 0) {
			return true;
		}
	}

	float value = adj->get_value () + offset;
	value       = std::max<float> (0, value);
	value       = std::min<float> (value, adj->get_upper () - adj->get_page_size ());
	adj->set_value (value);

	return true;
}

void
EditorSections::update_time_selection ()
{
	_view.get_selection ()->unselect_all ();

	Selection& selection (PublicEditor::instance ().get_selection ());

	if (selection.time.empty ()) {
		return;
	}

	Locations* loc = _session->locations ();
	Location*  l   = NULL;

	std::vector<Locations::LocationPair> locs;
	do {
		timepos_t start, end;
		l = loc->next_section_iter (l, start, end, locs);
		if (l) {
			if (start == selection.time.start_time () && end == selection.time.end_time ()) {
				LocationRowMap::iterator map_it = _location_row_map.find (l);
				TreeModel::iterator      j      = map_it->second;
				_view.get_selection ()->select (*j);
			}
		}
	} while (l);
}

void
EditorSections::selection_changed ()
{
	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	if (rows.empty ()) {
		return;
	}
	Gtk::TreeModel::Row row = *_model->get_iter (*rows.begin ());

	timepos_t start = row[_columns.start];
	timepos_t end   = row[_columns.end];

	_selection_change.block ();

	switch (PublicEditor::instance ().current_mouse_mode ()) {
		case Editing::MouseRange:
			/* OK */
			break;
		case Editing::MouseObject:
			if (ActionManager::get_toggle_action ("MouseMode", "set-mouse-mode-object-range")->get_active ()) {
				/* smart mode; OK */
				break;
			}
			/*fallthrough*/
		default:
			Glib::RefPtr<RadioAction> ract = ActionManager::get_radio_action (X_("MouseMode"), X_("set-mouse-mode-range"));
			ract->set_active (true);
			break;
	}

	Selection& s (PublicEditor::instance ().get_selection ());
	s.clear ();
	s.set (start, end);

	if (UIConfiguration::instance ().get_follow_edits ()) {
		_session->request_locate (start.samples());
	}

	_selection_change.unblock ();
}

void
EditorSections::drag_begin (Glib::RefPtr<Gdk::DragContext> const& context)
{
	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	if (!rows.empty ()) {
		Glib::RefPtr<Gdk::Pixmap> pix = _view.create_row_drag_icon (*rows.begin ());

		int w, h;
		pix->get_size (w, h);
		context->set_icon (pix->get_colormap (), pix, Glib::RefPtr<Gdk::Bitmap> (), 4, h / 2);
	}
}

void
EditorSections::drag_data_get (Glib::RefPtr<Gdk::DragContext> const&, Gtk::SelectionData& data, guint, guint)
{
	if (data.get_target () != "x-ardour/section") {
		return;
	}

	data.set (data.get_target (), 8, NULL, 0);
	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	for (auto const& r : rows) {
		TreeIter i;
		if ((i = _model->get_iter (r))) {
			Section s ((*i)[_columns.location], (*i)[_columns.start], (*i)[_columns.end]);
			data.set (data.get_target (), sizeof (Section), (guchar const*)&s, sizeof (Section));
			break;
		}
	}
}

bool
EditorSections::drag_motion (Glib::RefPtr<Gdk::DragContext> const& context, int x, int y, guint time)
{
	std::string const& target = _view.drag_dest_find_target (context, _view.drag_dest_get_target_list ());

	if (target != "x-ardour/section") {
		context->drag_status (Gdk::DragAction (0), time);
		return false;
	}

	int unused, header_height;
	_view.convert_bin_window_to_widget_coords (0, 0, unused, header_height);

	if (y < header_height) {
		context->drag_status (Gdk::DragAction (0), time);
		return false;
	}

	TreeModel::Path      path;
	TreeViewDropPosition pos;

	if (!_view.get_dest_row_at_pos (x, y, path, pos)) {
		assert (_model->children ().size () > 0);
		pos  = Gtk::TREE_VIEW_DROP_AFTER;
		path = TreeModel::Path ();
		path.push_back (_model->children ().size () - 1);
	}

	context->drag_status (context->get_suggested_action (), time);

	_view.set_drag_dest_row (path, pos);
	_view.drag_highlight ();

	if (!_scroll_timeout.connected ()) {
		_scroll_timeout = Glib::signal_timeout ().connect (sigc::mem_fun (*this, &EditorSections::scroll_row_timeout), 150);
	}

	return true;
}

void
EditorSections::drag_leave (Glib::RefPtr<Gdk::DragContext> const&, guint)
{
	_view.drag_unhighlight ();
	_scroll_timeout.disconnect ();
}

void
EditorSections::drag_data_received (Glib::RefPtr<Gdk::DragContext> const& context, int x, int y, Gtk::SelectionData const& data, guint /*info*/, guint time)
{
	if (data.get_target () != "x-ardour/section") {
		return;
	}
	if (data.get_length () != sizeof (Section)) {
		return;
	}

	SectionOperation op = CopyPasteSection;
	timepos_t        to (0);

	if ((context->get_suggested_action () == Gdk::ACTION_MOVE)) {
		op = CutPasteSection;
	}

	TreeModel::Path      path;
	TreeViewDropPosition pos;

	if (!_view.get_dest_row_at_pos (x, y, path, pos)) {
		/* paste at end */
		TreeModel::Children rows = _model->children ();
		assert (!rows.empty ());
		Gtk::TreeModel::Row row = *rows.rbegin ();
		to                      = row[_columns.end];
#ifndef NDEBUG
		cout << "EditorSections::drag_data_received - paste at end\n";
#endif
	} else {
		Gtk::TreeModel::iterator i = _model->get_iter (path);
		assert (i);
		if (pos == Gtk::TREE_VIEW_DROP_AFTER) {
#ifndef NDEBUG
			Location* loc = (*i)[_columns.location];
			cout << "EditorSections::drag_data_received - paste after '" << loc->name () << "'\n";
#endif
			to = (*i)[_columns.end];
		} else {
#ifndef NDEBUG
			Location* loc = (*i)[_columns.location];
			cout << "EditorSections::drag_data_received - paste before '" << loc->name () << "'\n";
#endif
			to = (*i)[_columns.start];
		}
	}

	/* Section is POD, memcpy is fine.
	 * data is free()ed by ~Gtk::SelectionData */
	Section s;
	memcpy ((void*) &s, data.get_data (), sizeof (Section));

	if (op == CutPasteSection && to > s.start) {
		/* offset/ripple `to` when using CutPasteSection */
		to = to.earlier (s.start.distance (s.end));
	}

#ifndef NDEBUG
	cout << "cut copy '" << s.location->name () << "' " << s.start << " - " << s.end << " to " << to << " op = " << op << "\n";
#endif
	{
		PBD::Unwinder<bool> uw (_no_redisplay, true);
		_session->cut_copy_section (s.start, s.end, to, op);
	}
	redisplay ();
}

bool
EditorSections::rename_selected_section ()
{
	if (_view.get_selection ()->count_selected_rows () != 1) {
		return false;
	}

	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();

	_view.set_cursor (*rows.begin (), *_view.get_column (0), true);

	return true;
}

bool
EditorSections::delete_selected_section ()
{
	if (_view.get_selection ()->count_selected_rows () != 1) {
		return false;
	}

	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	Gtk::TreeModel::Row                  row  = *_model->get_iter (*rows.begin ());

	timepos_t start = row[_columns.start];
	timepos_t end   = row[_columns.end];
	{
		PBD::Unwinder<bool> uw (_no_redisplay, true);
		_session->cut_copy_section (start, end, timepos_t (0), DeleteSection);
	}
	redisplay ();

	PublicEditor::instance ().get_selection ().clear ();

	return true;
}

bool
EditorSections::key_press (GdkEventKey* ev)
{
	switch (ev->keyval) {
		case GDK_KP_Delete:
			/* fallthrough */
		case GDK_Delete:
			/* fallthrough */
		case GDK_BackSpace:
			break;
		default:
			return false;
	}

	return delete_selected_section ();
}

void
EditorSections::show_context_menu (int button, int time)
{
	using namespace Gtk::Menu_Helpers;
	Gtk::Menu* menu  = ARDOUR_UI_UTILS::shared_popup_menu ();
	MenuList&  items = menu->items ();
	items.push_back (MenuElem (_("Rename the selected Section"), hide_return (sigc::mem_fun (*this, &EditorSections::rename_selected_section))));
	items.push_back (SeparatorElem ());
	items.push_back (MenuElem (_("Remove the selected Section"), hide_return (sigc::mem_fun (*this, &EditorSections::delete_selected_section))));
	menu->popup (button, time);
}

bool
EditorSections::button_press (GdkEventButton* ev)
{
	TreeModel::Path path;
	TreeViewColumn* column;
	int             cellx;
	int             celly;

	if (!_view.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
		TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
		assert (!rows.empty ());
		Gtk::TreeModel::Row row = *_model->get_iter (*rows.begin ());

		if (column == _view.get_column (1)) {
			timepos_t start = row[_columns.start];
			_session->request_locate (start.samples());
		} else if (column == _view.get_column (2)) {
			timepos_t end   = row[_columns.end];
			_session->request_locate (end.samples());
		} else {
			/* double-click edits name even with `mouse-edits-require-mod1` stack */
			_view.set_cursor (*rows.begin (), *_view.get_column(0), true);
			return true;
		}
		return false;
	}

	if (Gtkmm2ext::Keyboard::is_context_menu_event (ev)) {
		show_context_menu (ev->button, ev->time);
		/* return false to select item under the mouse */
	}
	return false;
}

void
EditorSections::name_edited (const std::string& path, const std::string& new_text)
{
	TreeIter i;
	if ((i = _model->get_iter (path))) {
		Location* l = (*i)[_columns.location];
		l->set_name (new_text);
	}
}

bool
EditorSections::enter_notify (GdkEventCrossing*)
{
	Gtkmm2ext::Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
EditorSections::leave_notify (GdkEventCrossing* ev)
{
	if (ev->detail != GDK_NOTIFY_INFERIOR && ev->detail != GDK_NOTIFY_ANCESTOR) {
		Gtkmm2ext::Keyboard::magic_widget_drop_focus ();
	}

	return false;
}
