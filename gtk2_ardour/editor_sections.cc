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
#include "editor_sections.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "main_clock.h"
#include "public_editor.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace ARDOUR;

EditorSections::EditorSections ()
	: _old_focus (0)
	, _no_redisplay (false)
{
	_model = ListStore::create (_columns);
	_view.set_model (_model);
	_view.append_column (_("Name"), _columns.name);
	_view.append_column (_("Start"), _columns.s_start);
	_view.append_column (_("End"), _columns.s_end);
	_view.set_headers_visible (true);
	_view.get_selection ()->set_mode (Gtk::SELECTION_SINGLE);

	_scroller.add (_view);
	_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	_view.signal_key_release_event ().connect (sigc::mem_fun (*this, &EditorSections::key_release), false);
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
	_scroller.signal_focus_in_event ().connect (sigc::mem_fun (*this, &EditorSections::focus_in), false);
	_scroller.signal_focus_out_event ().connect (sigc::mem_fun (*this, &EditorSections::focus_out));

	ARDOUR_UI::instance ()->primary_clock->mode_changed.connect (sigc::mem_fun (*this, &EditorSections::clock_format_changed));
}

void
EditorSections::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {
		_session->locations ()->added.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::redisplay, this), gui_context ());
		_session->locations ()->removed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::redisplay, this), gui_context ());
		_session->locations ()->changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::redisplay, this), gui_context ());

		Location::start_changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::redisplay, this), gui_context ());
		Location::end_changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::redisplay, this), gui_context ());
		Location::flags_changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::redisplay, this), gui_context ());
		Location::name_changed.connect (_session_connections, invalidator (*this), boost::bind (&EditorSections::redisplay, this), gui_context ());
	}

	redisplay ();
}

void
EditorSections::redisplay ()
{
	if (_no_redisplay) {
		return;
	}
	_view.set_model (Glib::RefPtr<ListStore> ());
	_model->clear ();

	if (_session == 0) {
		return;
	}

	timepos_t start;
	timepos_t end;

	Locations* loc = _session->locations ();
	Location*  l   = NULL;

	do {
		l = loc->next_section (l, start, end);
		if (l) {
			TreeModel::Row newrow     = *(_model->append ());
			newrow[_columns.name]     = l->name ();
			newrow[_columns.location] = l;
			newrow[_columns.start]    = start;
			newrow[_columns.end]      = end;
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
EditorSections::selection_changed ()
{
	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	if (rows.empty ()) {
		return;
	}
	Gtk::TreeModel::Row row = *_model->get_iter (*rows.begin ());

	timepos_t start = row[_columns.start];
	timepos_t end   = row[_columns.end];

	Selection& s (PublicEditor::instance ().get_selection ());
	s.set (start, end);
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
	memcpy (&s, data.get_data (), sizeof (Section));

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
EditorSections::key_release (GdkEventKey* ev)
{
	if (_view.get_selection ()->count_selected_rows () != 1) {
		return false;
	}

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

	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	Gtk::TreeModel::Row                  row  = *_model->get_iter (*rows.begin ());

	timepos_t start = row[_columns.start];
	timepos_t end   = row[_columns.end];
	{
		PBD::Unwinder<bool> uw (_no_redisplay, true);
		_session->cut_copy_section (start, end, timepos_t (0), DeleteSection);
	}
	redisplay ();
	return true;
}

bool
EditorSections::focus_in (GdkEventFocus*)
{
	Window* win = dynamic_cast<Window*> (_scroller.get_toplevel ());

	if (win) {
		_old_focus = win->get_focus ();
	} else {
		_old_focus = 0;
	}

	/* try to do nothing on focus in (doesn't work, hence selection_count nonsense) */
	return true;
}

bool
EditorSections::focus_out (GdkEventFocus*)
{
	if (_old_focus) {
		_old_focus->grab_focus ();
		_old_focus = 0;
	}
	return false;
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
	if (_old_focus) {
		_old_focus->grab_focus ();
		_old_focus = 0;
	}

	if (ev->detail != GDK_NOTIFY_INFERIOR && ev->detail != GDK_NOTIFY_ANCESTOR) {
		Gtkmm2ext::Keyboard::magic_widget_drop_focus ();
	}

	return false;
}
