/*
    Copyright (C) 2000-2009 Paul Davis

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
#include <cassert>
#include <cmath>
#include <list>
#include <vector>
#include <algorithm>

#include "pbd/unknown_type.h"
#include "pbd/unwind.h"

#include "ardour/debug.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/utils.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/cell_renderer_pixbuf_multi.h"
#include "gtkmm2ext/cell_renderer_pixbuf_toggle.h"
#include "gtkmm2ext/treeutils.h"

#include "actions.h"
#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "editor.h"
#include "editor_group_tabs.h"
#include "editor_routes.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "midi_time_axis.h"
#include "mixer_strip.h"
#include "plugin_setup_dialog.h"
#include "route_sorter.h"
#include "tooltips.h"
#include "vca_time_axis.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using Gtkmm2ext::Keyboard;

struct ColumnInfo {
	int         index;
	const char* label;
	const char* tooltip;
};

EditorRoutes::EditorRoutes (Editor* e)
	: EditorComponent (e)
	, _ignore_reorder (false)
	, _no_redisplay (false)
	, _adding_routes (false)
	, _route_deletion_in_progress (false)
	, _redisplay_on_resume (false)
	, _redisplay_active (0)
	, _queue_tv_update (0)
	, _menu (0)
	, old_focus (0)
	, selection_countdown (0)
	, name_editable (0)
{
	static const int column_width = 22;

	_scroller.add (_display);
	_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	_model = ListStore::create (_columns);
	_display.set_model (_model);

	// Record enable toggle
	CellRendererPixbufMulti* rec_col_renderer = manage (new CellRendererPixbufMulti());

	rec_col_renderer->set_pixbuf (0, ::get_icon("record-normal-disabled"));
	rec_col_renderer->set_pixbuf (1, ::get_icon("record-normal-in-progress"));
	rec_col_renderer->set_pixbuf (2, ::get_icon("record-normal-enabled"));
	rec_col_renderer->set_pixbuf (3, ::get_icon("record-step"));
	rec_col_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_rec_enable_changed));

	TreeViewColumn* rec_state_column = manage (new TreeViewColumn("R", *rec_col_renderer));

	rec_state_column->add_attribute(rec_col_renderer->property_state(), _columns.rec_state);
	rec_state_column->add_attribute(rec_col_renderer->property_visible(), _columns.is_track);

	rec_state_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	rec_state_column->set_alignment(ALIGN_CENTER);
	rec_state_column->set_expand(false);
	rec_state_column->set_fixed_width(column_width);


	// Record safe toggle
	CellRendererPixbufMulti* rec_safe_renderer = manage (new CellRendererPixbufMulti ());

	rec_safe_renderer->set_pixbuf (0, ::get_icon("rec-safe-disabled"));
	rec_safe_renderer->set_pixbuf (1, ::get_icon("rec-safe-enabled"));
	rec_safe_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_rec_safe_toggled));

	TreeViewColumn* rec_safe_column = manage (new TreeViewColumn(_("RS"), *rec_safe_renderer));
	rec_safe_column->add_attribute(rec_safe_renderer->property_state(), _columns.rec_safe);
	rec_safe_column->add_attribute(rec_safe_renderer->property_visible(), _columns.is_track);
	rec_safe_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	rec_safe_column->set_alignment(ALIGN_CENTER);
	rec_safe_column->set_expand(false);
	rec_safe_column->set_fixed_width(column_width);


	// MIDI Input Active

	CellRendererPixbufMulti* input_active_col_renderer = manage (new CellRendererPixbufMulti());
	input_active_col_renderer->set_pixbuf (0, ::get_icon("midi-input-inactive"));
	input_active_col_renderer->set_pixbuf (1, ::get_icon("midi-input-active"));
	input_active_col_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_input_active_changed));

	TreeViewColumn* input_active_column = manage (new TreeViewColumn ("I", *input_active_col_renderer));

	input_active_column->add_attribute(input_active_col_renderer->property_state(), _columns.is_input_active);
	input_active_column->add_attribute (input_active_col_renderer->property_visible(), _columns.is_midi);

	input_active_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	input_active_column->set_alignment(ALIGN_CENTER);
	input_active_column->set_expand(false);
	input_active_column->set_fixed_width(column_width);

	// Mute enable toggle
	CellRendererPixbufMulti* mute_col_renderer = manage (new CellRendererPixbufMulti());

	mute_col_renderer->set_pixbuf (Gtkmm2ext::Off, ::get_icon("mute-disabled"));
	mute_col_renderer->set_pixbuf (Gtkmm2ext::ImplicitActive, ::get_icon("muted-by-others"));
	mute_col_renderer->set_pixbuf (Gtkmm2ext::ExplicitActive, ::get_icon("mute-enabled"));
	mute_col_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_mute_enable_toggled));

	TreeViewColumn* mute_state_column = manage (new TreeViewColumn("M", *mute_col_renderer));

	mute_state_column->add_attribute(mute_col_renderer->property_state(), _columns.mute_state);
	mute_state_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	mute_state_column->set_alignment(ALIGN_CENTER);
	mute_state_column->set_expand(false);
	mute_state_column->set_fixed_width(15);

	// Solo enable toggle
	CellRendererPixbufMulti* solo_col_renderer = manage (new CellRendererPixbufMulti());

	solo_col_renderer->set_pixbuf (Gtkmm2ext::Off, ::get_icon("solo-disabled"));
	solo_col_renderer->set_pixbuf (Gtkmm2ext::ExplicitActive, ::get_icon("solo-enabled"));
	solo_col_renderer->set_pixbuf (Gtkmm2ext::ImplicitActive, ::get_icon("soloed-by-others"));
	solo_col_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_solo_enable_toggled));

	TreeViewColumn* solo_state_column = manage (new TreeViewColumn("S", *solo_col_renderer));

	solo_state_column->add_attribute(solo_col_renderer->property_state(), _columns.solo_state);
	solo_state_column->add_attribute(solo_col_renderer->property_visible(), _columns.solo_visible);
	solo_state_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	solo_state_column->set_alignment(ALIGN_CENTER);
	solo_state_column->set_expand(false);
	solo_state_column->set_fixed_width(column_width);

	// Solo isolate toggle
	CellRendererPixbufMulti* solo_iso_renderer = manage (new CellRendererPixbufMulti());

	solo_iso_renderer->set_pixbuf (0, ::get_icon("solo-isolate-disabled"));
	solo_iso_renderer->set_pixbuf (1, ::get_icon("solo-isolate-enabled"));
	solo_iso_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_solo_isolate_toggled));

	TreeViewColumn* solo_isolate_state_column = manage (new TreeViewColumn("SI", *solo_iso_renderer));

	solo_isolate_state_column->add_attribute(solo_iso_renderer->property_state(), _columns.solo_isolate_state);
	solo_isolate_state_column->add_attribute(solo_iso_renderer->property_visible(), _columns.solo_visible);
	solo_isolate_state_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	solo_isolate_state_column->set_alignment(ALIGN_CENTER);
	solo_isolate_state_column->set_expand(false);
	solo_isolate_state_column->set_fixed_width(column_width);

	// Solo safe toggle
	CellRendererPixbufMulti* solo_safe_renderer = manage (new CellRendererPixbufMulti ());

	solo_safe_renderer->set_pixbuf (0, ::get_icon("solo-safe-disabled"));
	solo_safe_renderer->set_pixbuf (1, ::get_icon("solo-safe-enabled"));
	solo_safe_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_solo_safe_toggled));

	TreeViewColumn* solo_safe_state_column = manage (new TreeViewColumn(_("SS"), *solo_safe_renderer));
	solo_safe_state_column->add_attribute(solo_safe_renderer->property_state(), _columns.solo_safe_state);
	solo_safe_state_column->add_attribute(solo_safe_renderer->property_visible(), _columns.solo_visible);
	solo_safe_state_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	solo_safe_state_column->set_alignment(ALIGN_CENTER);
	solo_safe_state_column->set_expand(false);
	solo_safe_state_column->set_fixed_width(column_width);

	_name_column = _display.append_column ("", _columns.text) - 1;
	_visible_column = _display.append_column ("", _columns.visible) - 1;
	_active_column = _display.append_column ("", _columns.active) - 1;

	_display.append_column (*input_active_column);
	_display.append_column (*rec_state_column);
	_display.append_column (*rec_safe_column);
	_display.append_column (*mute_state_column);
	_display.append_column (*solo_state_column);
	_display.append_column (*solo_isolate_state_column);
	_display.append_column (*solo_safe_state_column);


	TreeViewColumn* col;
	Gtk::Label* l;

	ColumnInfo ci[] = {
		{ 0,  _("Name"),        _("Track/Bus Name") },
		{ 1, S_("Visible|V"),   _("Track/Bus visible ?") },
		{ 2, S_("Active|A"),    _("Track/Bus active ?") },
		{ 3, S_("MidiInput|I"), _("MIDI input enabled") },
		{ 4, S_("Rec|R"),       _("Record enabled") },
		{ 5, S_("Rec|RS"),      _("Record Safe") },
		{ 6, S_("Mute|M"),      _("Muted") },
		{ 7, S_("Solo|S"),      _("Soloed") },
		{ 8, S_("SoloIso|SI"),  _("Solo Isolated") },
		{ 9, S_("SoloLock|SS"), _("Solo Safe (Locked)") },
		{ -1, 0, 0 }
	};

	for (int i = 0; ci[i].index >= 0; ++i) {
		col = _display.get_column (ci[i].index);
		l = manage (new Label (ci[i].label));
		set_tooltip (*l, ci[i].tooltip);
		col->set_widget (*l);
		l->show ();
	}

	_display.set_headers_visible (true);
	_display.get_selection()->set_mode (SELECTION_SINGLE);
	_display.get_selection()->set_select_function (sigc::mem_fun (*this, &EditorRoutes::selection_filter));
	_display.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::selection_changed));
	_display.set_reorderable (true);
	_display.set_name (X_("EditGroupList"));
	_display.set_rules_hint (true);
	_display.set_size_request (100, -1);
	_display.add_object_drag (_columns.stripable.index(), "routes");

	CellRendererText* name_cell = dynamic_cast<CellRendererText*> (_display.get_column_cell_renderer (_name_column));

	assert (name_cell);
	name_cell->signal_editing_started().connect (sigc::mem_fun (*this, &EditorRoutes::name_edit_started));

	TreeViewColumn* name_column = _display.get_column (_name_column);

	assert (name_column);

	name_column->add_attribute (name_cell->property_editable(), _columns.name_editable);
	name_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	name_column->set_expand(true);
	name_column->set_min_width(50);

	name_cell->property_editable() = true;
	name_cell->signal_edited().connect (sigc::mem_fun (*this, &EditorRoutes::name_edit));

	// Set the visible column cell renderer to radio toggle
	CellRendererToggle* visible_cell = dynamic_cast<CellRendererToggle*> (_display.get_column_cell_renderer (_visible_column));

	visible_cell->property_activatable() = true;
	visible_cell->property_radio() = false;
	visible_cell->signal_toggled().connect (sigc::mem_fun (*this, &EditorRoutes::visible_changed));

	TreeViewColumn* visible_col = dynamic_cast<TreeViewColumn*> (_display.get_column (_visible_column));
	visible_col->set_expand(false);
	visible_col->set_sizing(TREE_VIEW_COLUMN_FIXED);
	visible_col->set_fixed_width(30);
	visible_col->set_alignment(ALIGN_CENTER);

	CellRendererToggle* active_cell = dynamic_cast<CellRendererToggle*> (_display.get_column_cell_renderer (_active_column));

	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;
	active_cell->signal_toggled().connect (sigc::mem_fun (*this, &EditorRoutes::active_changed));

	TreeViewColumn* active_col = dynamic_cast<TreeViewColumn*> (_display.get_column (_active_column));
	active_col->set_expand (false);
	active_col->set_sizing (TREE_VIEW_COLUMN_FIXED);
	active_col->set_fixed_width (30);
	active_col->set_alignment (ALIGN_CENTER);

	_model->signal_row_deleted().connect (sigc::mem_fun (*this, &EditorRoutes::row_deleted));
	_model->signal_rows_reordered().connect (sigc::mem_fun (*this, &EditorRoutes::reordered));

	_display.signal_button_press_event().connect (sigc::mem_fun (*this, &EditorRoutes::button_press), false);
	_scroller.signal_key_press_event().connect (sigc::mem_fun(*this, &EditorRoutes::key_press), false);

	_scroller.signal_focus_in_event().connect (sigc::mem_fun (*this, &EditorRoutes::focus_in), false);
	_scroller.signal_focus_out_event().connect (sigc::mem_fun (*this, &EditorRoutes::focus_out));

	_display.signal_enter_notify_event().connect (sigc::mem_fun (*this, &EditorRoutes::enter_notify), false);
	_display.signal_leave_notify_event().connect (sigc::mem_fun (*this, &EditorRoutes::leave_notify), false);

	_display.set_enable_search (false);

	Route::PluginSetup.connect_same_thread (*this, boost::bind (&EditorRoutes::plugin_setup, this, _1, _2, _3));
	PresentationInfo::Change.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::sync_treeview_from_presentation_info, this), gui_context());
}

bool
EditorRoutes::focus_in (GdkEventFocus*)
{
	Window* win = dynamic_cast<Window*> (_scroller.get_toplevel ());

	if (win) {
		old_focus = win->get_focus ();
	} else {
		old_focus = 0;
	}

	name_editable = 0;

	/* try to do nothing on focus in (doesn't work, hence selection_count nonsense) */
	return true;
}

bool
EditorRoutes::focus_out (GdkEventFocus*)
{
	if (old_focus) {
		old_focus->grab_focus ();
		old_focus = 0;
	}

	return false;
}

bool
EditorRoutes::enter_notify (GdkEventCrossing*)
{
	if (name_editable) {
		return true;
	}

	/* arm counter so that ::selection_filter() will deny selecting anything for the
	 * next two attempts to change selection status.
	 */
	selection_countdown = 2;
	_scroller.grab_focus ();
	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
EditorRoutes::leave_notify (GdkEventCrossing*)
{
	selection_countdown = 0;

	if (old_focus) {
		old_focus->grab_focus ();
		old_focus = 0;
	}

	Keyboard::magic_widget_drop_focus ();
	return false;
}

void
EditorRoutes::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	initial_display ();

	if (_session) {
		_session->SoloChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::solo_changed_so_update_mute, this), gui_context());
		_session->RecordStateChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_rec_display, this), gui_context());

		/* TODO: check if these needs to be tied in with DisplaySuspender
		 * Given that the UI is single-threaded and DisplaySuspender is only used
		 * in loops in the UI thread all should be fine.
		 */
		_session->BatchUpdateStart.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::suspend_redisplay, this), gui_context());
		_session->BatchUpdateEnd.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::resume_redisplay, this), gui_context());
	}
}

void
EditorRoutes::on_input_active_changed (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView* tv = row[_columns.tv];
	RouteTimeAxisView *rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (rtv) {
		boost::shared_ptr<MidiTrack> mt;
		mt = rtv->midi_track();
		if (mt) {
			mt->set_input_active (!mt->input_active());
		}
	}
}

void
EditorRoutes::on_tv_rec_enable_changed (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView* tv = row[_columns.tv];
	RouteTimeAxisView *rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (!rtv) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac = rtv->route()->rec_enable_control();

	if (ac) {
		ac->set_value (!ac->get_value(), Controllable::UseGroup);
	}
}

void
EditorRoutes::on_tv_rec_safe_toggled (std::string const & path_string)
{
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));
	TimeAxisView* tv = row[_columns.tv];
	RouteTimeAxisView *rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (!rtv) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (rtv->route()->rec_safe_control());

	if (ac) {
		ac->set_value (!ac->get_value(), Controllable::UseGroup);
	}
}

void
EditorRoutes::on_tv_mute_enable_toggled (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	RouteTimeAxisView *rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (!rtv) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (rtv->route()->mute_control());

	if (ac) {
		ac->set_value (!ac->get_value(), Controllable::UseGroup);
	}
}

void
EditorRoutes::on_tv_solo_enable_toggled (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (!rtv) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (rtv->route()->solo_control());

	if (ac) {
		ac->set_value (!ac->get_value(), Controllable::UseGroup);
	}
}

void
EditorRoutes::on_tv_solo_isolate_toggled (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (!rtv) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (rtv->route()->solo_isolate_control());

	if (ac) {
		ac->set_value (!ac->get_value(), Controllable::UseGroup);
	}
}

void
EditorRoutes::on_tv_solo_safe_toggled (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (!rtv) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (rtv->route()->solo_safe_control());

	if (ac) {
		ac->set_value (!ac->get_value(), Controllable::UseGroup);
	}
}

void
EditorRoutes::build_menu ()
{
	using namespace Menu_Helpers;
	using namespace Gtk;

	_menu = new Menu;

	MenuList& items = _menu->items();
	_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Show All"), sigc::mem_fun (*this, &EditorRoutes::show_all_routes)));
	items.push_back (MenuElem (_("Hide All"), sigc::mem_fun (*this, &EditorRoutes::hide_all_routes)));
	items.push_back (MenuElem (_("Show All Audio Tracks"), sigc::mem_fun (*this, &EditorRoutes::show_all_audiotracks)));
	items.push_back (MenuElem (_("Hide All Audio Tracks"), sigc::mem_fun (*this, &EditorRoutes::hide_all_audiotracks)));
	items.push_back (MenuElem (_("Show All Audio Busses"), sigc::mem_fun (*this, &EditorRoutes::show_all_audiobus)));
	items.push_back (MenuElem (_("Hide All Audio Busses"), sigc::mem_fun (*this, &EditorRoutes::hide_all_audiobus)));
	items.push_back (MenuElem (_("Show All Midi Tracks"), sigc::mem_fun (*this, &EditorRoutes::show_all_miditracks)));
	items.push_back (MenuElem (_("Hide All Midi Tracks"), sigc::mem_fun (*this, &EditorRoutes::hide_all_miditracks)));
	items.push_back (MenuElem (_("Only Show Tracks with Regions Under Playhead"), sigc::mem_fun (*this, &EditorRoutes::show_tracks_with_regions_at_playhead)));
}

void
EditorRoutes::show_menu ()
{
	if (_menu == 0) {
		build_menu ();
	}

	_menu->popup (1, gtk_get_current_event_time());
}

void
EditorRoutes::redisplay_real ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;
	uint32_t position;

	/* n will be the count of tracks plus children (updated by TimeAxisView::show_at),
	 * so we will use that to know where to put things.
	 */
	int n;

	for (n = 0, position = 0, i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[_columns.tv];

		if (tv == 0) {
			// just a "title" row
			continue;
		}

		bool visible = tv->marked_for_display ();

		/* show or hide the TimeAxisView */
		if (visible) {
			position += tv->show_at (position, n, &_editor->edit_controls_vbox);
		} else {
			tv->hide ();
		}

		n++;
	}

	/* whenever we go idle, update the track view list to reflect the new order.
	 * we can't do this here, because we could mess up something that is traversing
	 * the track order and has caused a redisplay of the list.
	 */
	Glib::signal_idle().connect (sigc::mem_fun (*_editor, &Editor::sync_track_view_list_and_routes));

	_editor->reset_controls_layout_height (position);
	_editor->reset_controls_layout_width ();
	_editor->_full_canvas_height = position;

	if ((_editor->vertical_adjustment.get_value() + _editor->_visible_canvas_height) > _editor->vertical_adjustment.get_upper()) {
		/*
		 * We're increasing the size of the canvas while the bottom is visible.
		 * We scroll down to keep in step with the controls layout.
		 */
		_editor->vertical_adjustment.set_value (_editor->_full_canvas_height - _editor->_visible_canvas_height);
	}
}

void
EditorRoutes::redisplay ()
{
	if (!_session || _session->deletion_in_progress()) {
		return;
	}

	if (_no_redisplay) {
		_redisplay_on_resume = true;
		return;
	}

	// model deprecated g_atomic_int_exchange_and_add(, 1)
	g_atomic_int_inc(const_cast<gint*>(&_redisplay_active));
	if (!g_atomic_int_compare_and_exchange (const_cast<gint*>(&_redisplay_active), 1, 1)) {
		/* recursive re-display can happen if redisplay shows/hides a TrackView
		 * which has children and their display status changes as result.
		 */
		return;
	}

	redisplay_real ();

	while (!g_atomic_int_compare_and_exchange (const_cast<gint*>(&_redisplay_active), 1, 0)) {
		g_atomic_int_set(const_cast<gint*>(&_redisplay_active), 1);
		redisplay_real ();
	}
}

void
EditorRoutes::row_deleted (Gtk::TreeModel::Path const &)
{
	if (!_session || _session->deletion_in_progress()) {
		return;
	}
	/* this happens as the second step of a DnD within the treeview, and
	 * when a route is actually removed. we don't differentiate between
	 * the two cases.
	 *
	 * note that the sync_presentation_info_from_treeview() step may not
	 * actually change any presentation info (e.g. the last track may be
	 * removed, so all other tracks keep the same presentation info), which
	 * means that no redisplay would happen. so we have to force a
	 * redisplay.
	 */

	DEBUG_TRACE (DEBUG::OrderKeys, "editor routes treeview row deleted\n");

	DisplaySuspender ds;
	sync_presentation_info_from_treeview ();
}

void
EditorRoutes::reordered (TreeModel::Path const &, TreeModel::iterator const &, int* /*what*/)
{
	/* reordering implies that RID's will change, so
	   sync_presentation_info_from_treeview() will cause a redisplay.
	*/

	DEBUG_TRACE (DEBUG::OrderKeys, "editor routes treeview reordered\n");
	sync_presentation_info_from_treeview ();
}

void
EditorRoutes::visible_changed (std::string const & path)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

	DisplaySuspender ds;
	TreeIter iter;

	if ((iter = _model->get_iter (path))) {
		TimeAxisView* tv = (*iter)[_columns.tv];
		if (tv) {
			bool visible = (*iter)[_columns.visible];

			if (tv->set_marked_for_display (!visible)) {
				update_visibility ();
			}
		}
	}
}

void
EditorRoutes::active_changed (std::string const & path)
{
	if (_session && _session->deletion_in_progress ()) {
		return;
	}

	Gtk::TreeModel::Row row = *_model->get_iter (path);
	boost::shared_ptr<Stripable> stripable = row[_columns.stripable];
	boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (stripable);
	if (route) {
		bool const active = row[_columns.active];
		route->set_active (!active, this);
	}
}

void
EditorRoutes::time_axis_views_added (list<TimeAxisView*> tavs)
{
	PBD::Unwinder<bool> at (_adding_routes, true);
	bool from_scratch = (_model->children().size() == 0);
	Gtk::TreeModel::Children::iterator insert_iter = _model->children().end();

	for (Gtk::TreeModel::Children::iterator it = _model->children().begin(); it != _model->children().end(); ++it) {

		boost::shared_ptr<Stripable> r = (*it)[_columns.stripable];

		if (r->presentation_info().order() == (tavs.front()->stripable()->presentation_info().order() + tavs.size())) {
			insert_iter = it;
			break;
		}
	}

	_display.set_model (Glib::RefPtr<ListStore>());

	for (list<TimeAxisView*>::iterator x = tavs.begin(); x != tavs.end(); ++x) {

		VCATimeAxisView* vtav = dynamic_cast<VCATimeAxisView*> (*x);
		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*x);

		TreeModel::Row row = *(_model->insert (insert_iter));

		boost::shared_ptr<Stripable> stripable;
		boost::shared_ptr<MidiTrack> midi_trk;

		if (vtav) {

			stripable = vtav->vca();

			row[_columns.is_track] = false;
			row[_columns.is_input_active] = false;
			row[_columns.is_midi] = false;

		} else if (rtav) {

			stripable = rtav->route ();
			midi_trk= boost::dynamic_pointer_cast<MidiTrack> (stripable);

			row[_columns.is_track] = (boost::dynamic_pointer_cast<Track> (stripable) != 0);


			if (midi_trk) {
				row[_columns.is_input_active] = midi_trk->input_active ();
				row[_columns.is_midi] = true;
			} else {
				row[_columns.is_input_active] = false;
				row[_columns.is_midi] = false;
			}
		}

		if (!stripable) {
			continue;
		}

		row[_columns.text] = stripable->name();
		row[_columns.visible] = (*x)->marked_for_display();
		row[_columns.active] = true;
		row[_columns.tv] = *x;
		row[_columns.stripable] = stripable;
		row[_columns.mute_state] = RouteUI::mute_active_state (_session, stripable);
		row[_columns.solo_state] = RouteUI::solo_active_state (stripable);
		row[_columns.solo_visible] = true;
		row[_columns.solo_isolate_state] = RouteUI::solo_isolate_active_state (stripable);
		row[_columns.solo_safe_state] = RouteUI::solo_safe_active_state (stripable);
		row[_columns.name_editable] = true;

		boost::weak_ptr<Stripable> ws (stripable);

		/* for now, we need both of these. PropertyChanged covers on
		 * pre-defined, "global" things of interest to a
		 * UI. gui_changed covers arbitrary, un-enumerated, un-typed
		 * changes that may only be of interest to a particular
		 * UI (e.g. track-height is not of any relevant to OSC)
		 */

		stripable->gui_changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::handle_gui_changes, this, _1, _2), gui_context());
		stripable->PropertyChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::route_property_changed, this, _1, ws), gui_context());
		stripable->presentation_info().PropertyChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::route_property_changed, this, _1, ws), gui_context());

		if (boost::dynamic_pointer_cast<Track> (stripable)) {
			boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (stripable);
			t->rec_enable_control()->Changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_rec_display, this), gui_context());
			t->rec_safe_control()->Changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_rec_display, this), gui_context());
		}

		if (midi_trk) {
			midi_trk->StepEditStatusChange.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_rec_display, this), gui_context());
			midi_trk->InputActiveChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_input_active_display, this), gui_context());
		}

		boost::shared_ptr<AutomationControl> ac;

		if ((ac = stripable->mute_control()) != 0) {
			ac->Changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_mute_display, this), gui_context());
		}
		if ((ac = stripable->solo_control()) != 0) {
			ac->Changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_solo_display, this), gui_context());
		}
		if ((ac = stripable->solo_isolate_control()) != 0) {
			ac->Changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_solo_isolate_display, this), gui_context());
		}
		if ((ac = stripable->solo_safe_control()) != 0) {
			ac->Changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_solo_safe_display, this), gui_context());
		}

		if (rtav) {
			rtav->route()->active_changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_active_display, this), gui_context ());
		}
	}

	update_rec_display ();
	update_mute_display ();
	update_solo_display ();
	update_solo_isolate_display ();
	update_solo_safe_display ();
	update_input_active_display ();
	update_active_display ();

	_display.set_model (_model);

	/* now update route order keys from the treeview/track display order */

	if (!from_scratch) {
		sync_presentation_info_from_treeview ();
	}

	redisplay ();
}

void
EditorRoutes::handle_gui_changes (string const & what, void*)
{
	if (_adding_routes) {
		return;
	}

	if (what == "track_height") {
		/* Optional :make tracks change height while it happens, instead
		   of on first-idle
		*/
		redisplay ();
	}

	if (what == "visible_tracks") {
		redisplay ();
	}
}

void
EditorRoutes::route_removed (TimeAxisView *tv)
{
	ENSURE_GUI_THREAD (*this, &EditorRoutes::route_removed, tv)

	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator ri;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[_columns.tv] == tv) {
			PBD::Unwinder<bool> uw (_route_deletion_in_progress, true);
			_model->erase (ri);
			break;
		}
	}

	/* the deleted signal for the treeview/model will take
	   care of any updates.
	*/
}

void
EditorRoutes::route_property_changed (const PropertyChange& what_changed, boost::weak_ptr<Stripable> s)
{
	if (!what_changed.contains (ARDOUR::Properties::hidden) && !what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}

	if (_adding_routes) {
		return;
	}

	boost::shared_ptr<Stripable> stripable = s.lock ();

	if (!stripable) {
		return;
	}

	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {

		boost::shared_ptr<Stripable> ss = (*i)[_columns.stripable];

		if (ss == stripable) {

			if (what_changed.contains (ARDOUR::Properties::name)) {
				(*i)[_columns.text] = stripable->name();
				break;
			}

			if (what_changed.contains (ARDOUR::Properties::hidden)) {
				(*i)[_columns.visible] = !stripable->presentation_info().hidden();
				cerr << stripable->name() << " visibility changed, redisplay\n";
				redisplay ();

			}

			break;
		}
	}
}

void
EditorRoutes::update_active_display ()
{
	if (g_atomic_int_compare_and_exchange (const_cast<gint*>(&_queue_tv_update), 0, 1)) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

void
EditorRoutes::update_visibility ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	DisplaySuspender ds;

	for (i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[_columns.tv];
		(*i)[_columns.visible] = tv->marked_for_display ();
	}

	/* force route order keys catch up with visibility changes
	 */

	sync_presentation_info_from_treeview ();
}

void
EditorRoutes::hide_track_in_display (TimeAxisView& tv)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[_columns.tv] == &tv) {
			tv.set_marked_for_display (false);
			(*i)[_columns.visible] = false;
			redisplay ();
			break;
		}
	}
}

void
EditorRoutes::show_track_in_display (TimeAxisView& tv)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;


	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[_columns.tv] == &tv) {
			tv.set_marked_for_display (true);
			(*i)[_columns.visible] = true;
			redisplay ();
			break;
		}
	}
}

void
EditorRoutes::sync_presentation_info_from_treeview ()
{
	if (_ignore_reorder || !_session || _session->deletion_in_progress()) {
		return;
	}

	TreeModel::Children rows = _model->children();

	if (rows.empty()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "editor sync presentation info from treeview\n");

	TreeModel::Children::iterator ri;
	bool change = false;
	PresentationInfo::order_t order = 0;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {

		boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
		bool visible = (*ri)[_columns.visible];

		/* Monitor and Auditioner do not get their presentation
		 * info reset here.
		 */

		if (stripable->is_monitor() || stripable->is_auditioner()) {
			continue;
		}

		stripable->presentation_info().set_hidden (!visible);

		if (order != stripable->presentation_info().order()) {
			stripable->set_presentation_order (order, false);
			change = true;
		}

		++order;
	}

	if (change) {
		DEBUG_TRACE (DEBUG::OrderKeys, "... notify PI change from editor GUI\n");
		_session->notify_presentation_info_change ();
	}
}

void
EditorRoutes::sync_treeview_from_presentation_info ()
{
	/* Some route order key(s) have been changed, make sure that
	   we update out tree/list model and GUI to reflect the change.
	*/

	if (_ignore_reorder || !_session || _session->deletion_in_progress()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "editor sync model from presentation info.\n");

	vector<int> neworder;
	TreeModel::Children rows = _model->children();
	uint32_t old_order = 0;
	bool changed = false;

	if (rows.empty()) {
		return;
	}

	OrderingKeys sorted;

	for (TreeModel::Children::iterator ri = rows.begin(); ri != rows.end(); ++ri, ++old_order) {
		boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
		/* use global order */
		sorted.push_back (OrderKeys (old_order, stripable->presentation_info().order()));
	}

	SortByNewDisplayOrder cmp;

	sort (sorted.begin(), sorted.end(), cmp);
	neworder.assign (sorted.size(), 0);

	uint32_t n = 0;

	for (OrderingKeys::iterator sr = sorted.begin(); sr != sorted.end(); ++sr, ++n) {

		neworder[n] = sr->old_display_order;

		if (sr->old_display_order != n) {
			changed = true;
		}
	}

	if (changed) {
		Unwinder<bool> uw (_ignore_reorder, true);
		_model->reorder (neworder);
	}

	redisplay ();
}

void
EditorRoutes::hide_all_tracks (bool /*with_select*/)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	DisplaySuspender ds;

	for (i = rows.begin(); i != rows.end(); ++i) {

		TreeModel::Row row = (*i);
		TimeAxisView *tv = row[_columns.tv];

		if (tv == 0) {
			continue;
		}

		row[_columns.visible] = false;
	}
}

void
EditorRoutes::set_all_tracks_visibility (bool yn)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	DisplaySuspender ds;

	for (i = rows.begin(); i != rows.end(); ++i) {

		TreeModel::Row row = (*i);
		TimeAxisView* tv = row[_columns.tv];

		if (tv == 0) {
			continue;
		}

		tv->set_marked_for_display (yn);
		(*i)[_columns.visible] = yn;
	}

	/* force route order keys catch up with visibility changes
	 */

	sync_presentation_info_from_treeview ();
}

void
EditorRoutes::set_all_audio_midi_visibility (int tracks, bool yn)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	DisplaySuspender ds;

	for (i = rows.begin(); i != rows.end(); ++i) {

		TreeModel::Row row = (*i);
		TimeAxisView* tv = row[_columns.tv];

		AudioTimeAxisView* atv;
		MidiTimeAxisView* mtv;

		if (tv == 0) {
			continue;
		}

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			switch (tracks) {
			case 0:
				atv->set_marked_for_display (yn);
				(*i)[_columns.visible] = yn;
				break;

			case 1:
				if (atv->is_audio_track()) {
					atv->set_marked_for_display (yn);
					(*i)[_columns.visible] = yn;
				}
				break;

			case 2:
				if (!atv->is_audio_track()) {
					atv->set_marked_for_display (yn);
					(*i)[_columns.visible] = yn;
				}
				break;
			}
		}
		else if ((mtv = dynamic_cast<MidiTimeAxisView*>(tv)) != 0) {
			switch (tracks) {
			case 0:
				mtv->set_marked_for_display (yn);
				(*i)[_columns.visible] = yn;
				break;

			case 3:
				if (mtv->is_midi_track()) {
					mtv->set_marked_for_display (yn);
					(*i)[_columns.visible] = yn;
				}
				break;
			}
		}
	}

	/* force route order keys catch up with visibility changes
	 */

	sync_presentation_info_from_treeview ();
}

void
EditorRoutes::hide_all_routes ()
{
	set_all_tracks_visibility (false);
}

void
EditorRoutes::show_all_routes ()
{
	set_all_tracks_visibility (true);
}

void
EditorRoutes::show_all_audiotracks()
{
	set_all_audio_midi_visibility (1, true);
}
void
EditorRoutes::hide_all_audiotracks ()
{
	set_all_audio_midi_visibility (1, false);
}

void
EditorRoutes::show_all_audiobus ()
{
	set_all_audio_midi_visibility (2, true);
}
void
EditorRoutes::hide_all_audiobus ()
{
	set_all_audio_midi_visibility (2, false);
}

void
EditorRoutes::show_all_miditracks()
{
	set_all_audio_midi_visibility (3, true);
}
void
EditorRoutes::hide_all_miditracks ()
{
	set_all_audio_midi_visibility (3, false);
}

bool
EditorRoutes::key_press (GdkEventKey* ev)
{
	TreeViewColumn *col;
	boost::shared_ptr<RouteList> rl (new RouteList);
	TreePath path;

	switch (ev->keyval) {
		case GDK_Tab:
		case GDK_ISO_Left_Tab:

			/* If we appear to be editing something, leave that cleanly and appropriately. */
			if (name_editable) {
				name_editable->editing_done ();
				name_editable = 0;
			}

			col = _display.get_column (_name_column); // select&focus on name column

			if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
				treeview_select_previous (_display, _model, col);
			} else {
				treeview_select_next (_display, _model, col);
			}

			return true;
			break;

		case 'm':
			if (get_relevant_routes (rl)) {
				_session->set_controls (route_list_to_control_list (rl, &Stripable::mute_control), rl->front()->muted() ? 0.0 : 1.0, Controllable::NoGroup);
			}
			return true;
			break;

		case 's':
			if (get_relevant_routes (rl)) {
				_session->set_controls (route_list_to_control_list (rl, &Stripable::solo_control), rl->front()->self_soloed() ? 0.0 : 1.0, Controllable::NoGroup);
			}
			return true;
			break;

		case 'r':
			if (get_relevant_routes (rl)) {
				for (RouteList::const_iterator r = rl->begin(); r != rl->end(); ++r) {
					boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (*r);
					if (t) {
						_session->set_controls (route_list_to_control_list (rl, &Stripable::rec_enable_control), !t->rec_enable_control()->get_value(), Controllable::NoGroup);
						break;
					}
				}
			}
			break;

		default:
			break;
	}

	return false;
}

bool
EditorRoutes::get_relevant_routes (boost::shared_ptr<RouteList> rl)
{
	TimeAxisView* tv;
	RouteTimeAxisView* rtv;
	RefPtr<TreeSelection> selection = _display.get_selection();
	TreePath path;
	TreeIter iter;

	if (selection->count_selected_rows() != 0) {

		/* use selection */

		RefPtr<TreeModel> tm = RefPtr<TreeModel>::cast_dynamic (_model);
		iter = selection->get_selected (tm);

	} else {
		/* use mouse pointer */

		int x, y;
		int bx, by;

		_display.get_pointer (x, y);
		_display.convert_widget_to_bin_window_coords (x, y, bx, by);

		if (_display.get_path_at_pos (bx, by, path)) {
			iter = _model->get_iter (path);
		}
	}

	if (iter) {
		tv = (*iter)[_columns.tv];
		if (tv) {
			rtv = dynamic_cast<RouteTimeAxisView*>(tv);
			if (rtv) {
				rl->push_back (rtv->route());
			}
		}
	}

	return !rl->empty();
}

bool
EditorRoutes::button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		show_menu ();
		return true;
	}

	TreeModel::Path path;
	TreeViewColumn *tvc;
	int cell_x;
	int cell_y;

	if (!_display.get_path_at_pos ((int) ev->x, (int) ev->y, path, tvc, cell_x, cell_y)) {
		/* cancel selection */
		_display.get_selection()->unselect_all ();
		/* end any editing by grabbing focus */
		_display.grab_focus ();
		return true;
	}

	//Scroll editor canvas to selected track
	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

		Gtk::TreeModel::Row row = *_model->get_iter (path);
		TimeAxisView *tv = row[_columns.tv];

		if (tv) {
			_editor->ensure_time_axis_view_is_visible (*tv, true);
		}
	}

	return false;
}

void
EditorRoutes::selection_changed ()
{
	_editor->begin_reversible_selection_op (X_("Select Track from Route List"));

	if (_display.get_selection()->count_selected_rows() > 0) {

		TreeIter iter;
		TreeView::Selection::ListHandle_Path rows = _display.get_selection()->get_selected_rows ();
		TrackViewList selected;

		_editor->get_selection().clear_regions ();

		for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {

			if ((iter = _model->get_iter (*i))) {

				TimeAxisView* tv = (*iter)[_columns.tv];
				selected.push_back (tv);
			}

		}

		_editor->get_selection().set (selected);
		_editor->ensure_time_axis_view_is_visible (*(selected.front()), true);

	} else {
		_editor->get_selection().clear_tracks ();
	}

	_editor->commit_reversible_selection_op ();
}

bool
EditorRoutes::selection_filter (Glib::RefPtr<TreeModel> const &, TreeModel::Path const&, bool /*selected*/)
{
	if (selection_countdown) {
		if (--selection_countdown == 0) {
			return true;
		} else {
			/* no selection yet ... */
			return false;
		}
	}
	return true;
}

struct PresentationInfoRouteSorter
{
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		if (a->is_master()) {
			/* master before everything else */
			return true;
		} else if (b->is_master()) {
			/* everything else before master */
			return false;
		}
		return a->presentation_info().order () < b->presentation_info().order ();
	}
};

struct PresentationInfoVCASorter
{
	bool operator() (boost::shared_ptr<VCA> a, boost::shared_ptr<VCA> b) {
		return a->presentation_info().order () < b->presentation_info().order ();
	}
};

void
EditorRoutes::initial_display ()
{
	DisplaySuspender ds;
	_model->clear ();

	if (!_session) {
		return;
	}

	StripableList s;

	RouteList r (*_session->get_routes());
	for (RouteList::iterator ri = r.begin(); ri != r.end(); ++ri) {
		s.push_back (*ri);
	}

	VCAList v (_session->vca_manager().vcas());
	for (VCAList::iterator vi = v.begin(); vi != v.end(); ++vi) {
		s.push_back (*vi);
	}

	_editor->add_stripables (s);
}

void
EditorRoutes::display_drag_data_received (const RefPtr<Gdk::DragContext>& context,
					     int x, int y,
					     const SelectionData& data,
					     guint info, guint time)
{
	if (data.get_target() == "GTK_TREE_MODEL_ROW") {
		_display.on_drag_data_received (context, x, y, data, info, time);
		return;
	}

	context->drag_finish (true, false, time);
}

struct ViewStripable {
	TimeAxisView* tav;
	boost::shared_ptr<Stripable> stripable;
	uint32_t old_order;

	ViewStripable (TimeAxisView* t, boost::shared_ptr<Stripable> s, uint32_t n)
		: tav (t), stripable (s), old_order (n) {}
};

void
EditorRoutes::move_selected_tracks (bool up)
{
	if (_editor->selection->tracks.empty()) {
		return;
	}

	std::list<ViewStripable> view_stripables;
	std::vector<int> neworder;
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator ri;
	TreeModel::Children::size_type n;

	for (n = 0, ri = rows.begin(); ri != rows.end(); ++ri, ++n) {
		TimeAxisView* tv = (*ri)[_columns.tv];
		boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
		view_stripables.push_back (ViewStripable (tv, stripable, n));
	}

	list<ViewStripable>::iterator trailing;
	list<ViewStripable>::iterator leading;

	if (up) {

		trailing = view_stripables.begin();
		leading = view_stripables.begin();

		++leading;

		while (leading != view_stripables.end()) {
			if (_editor->selection->selected (leading->tav)) {
				view_stripables.insert (trailing, ViewStripable (*leading));
				leading = view_stripables.erase (leading);
			} else {
				++leading;
				++trailing;
			}
		}

	} else {

		/* if we could use reverse_iterator in list::insert, this code
		   would be a beautiful reflection of the code above. but we can't
		   and so it looks like a bit of a mess.
		*/

		trailing = view_stripables.end();
		leading = view_stripables.end();

		--leading; if (leading == view_stripables.begin()) { return; }
		--leading;
		--trailing;

		while (1) {

			if (_editor->selection->selected (leading->tav)) {
				list<ViewStripable>::iterator tmp;

				/* need to insert *after* trailing, not *before* it,
				   which is what insert (iter, val) normally does.
				*/

				tmp = trailing;
				tmp++;

				view_stripables.insert (tmp, ViewStripable (*leading));

				/* can't use iter = cont.erase (iter); form here, because
				   we need iter to move backwards.
				*/

				tmp = leading;
				--tmp;

				bool done = false;

				if (leading == view_stripables.begin()) {
					/* the one we've just inserted somewhere else
					   was the first in the list. erase this copy,
					   and then break, because we're done.
					*/
					done = true;
				}

				view_stripables.erase (leading);

				if (done) {
					break;
				}

				leading = tmp;

			} else {
				if (leading == view_stripables.begin()) {
					break;
				}
				--leading;
				--trailing;
			}
		};
	}

	for (leading = view_stripables.begin(); leading != view_stripables.end(); ++leading) {
		neworder.push_back (leading->old_order);
#ifndef NDEBUG
		if (leading->old_order != neworder.size() - 1) {
			DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("move %1 to %2\n", leading->old_order, neworder.size() - 1));
		}
#endif

	}
#ifndef NDEBUG
	DEBUG_TRACE (DEBUG::OrderKeys, "New order after moving tracks:\n");
	for (vector<int>::iterator i = neworder.begin(); i != neworder.end(); ++i) {
		DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("\t%1\n", *i));
	}
	DEBUG_TRACE (DEBUG::OrderKeys, "-------\n");
#endif


	_model->reorder (neworder);
}

void
EditorRoutes::update_input_active_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Stripable> stripable = (*i)[_columns.stripable];

		if (boost::dynamic_pointer_cast<Track> (stripable)) {
			boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (stripable);

			if (mt) {
				(*i)[_columns.is_input_active] = mt->input_active();
			}
		}
	}
}

void
EditorRoutes::update_rec_display ()
{
	if (g_atomic_int_compare_and_exchange (const_cast<gint*>(&_queue_tv_update), 0, 1)) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

bool
EditorRoutes::idle_update_mute_rec_solo_etc()
{
	g_atomic_int_set (const_cast<gint*>(&_queue_tv_update), 0);
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Stripable> stripable = (*i)[_columns.stripable];
		boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (stripable);
		(*i)[_columns.mute_state] = RouteUI::mute_active_state (_session, stripable);
		(*i)[_columns.solo_state] = RouteUI::solo_active_state (stripable);
		(*i)[_columns.solo_isolate_state] = RouteUI::solo_isolate_active_state (stripable) ? 1 : 0;
		(*i)[_columns.solo_safe_state] = RouteUI::solo_safe_active_state (stripable) ? 1 : 0;
		if (route) {
			(*i)[_columns.active] = route->active ();
		} else {
			(*i)[_columns.active] = true;
		}

		boost::shared_ptr<Track> trk (boost::dynamic_pointer_cast<Track>(route));

		if (trk) {
			boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (route);

			if (trk->rec_enable_control()->get_value()) {
				if (_session->record_status() == Session::Recording) {
					(*i)[_columns.rec_state] = 1;
				} else {
					(*i)[_columns.rec_state] = 2;
				}
			} else if (mt && mt->step_editing()) {
				(*i)[_columns.rec_state] = 3;
			} else {
				(*i)[_columns.rec_state] = 0;
			}

			(*i)[_columns.rec_safe] = trk->rec_safe_control()->get_value();
			(*i)[_columns.name_editable] = !trk->rec_enable_control()->get_value();
		}
	}

	return false; // do not call again (until needed)
}


void
EditorRoutes::update_mute_display ()
{
	if (g_atomic_int_compare_and_exchange (const_cast<gint*>(&_queue_tv_update), 0, 1)) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

void
EditorRoutes::update_solo_display ()
{
	if (g_atomic_int_compare_and_exchange (const_cast<gint*>(&_queue_tv_update), 0, 1)) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

void
EditorRoutes::update_solo_isolate_display ()
{
	if (g_atomic_int_compare_and_exchange (const_cast<gint*>(&_queue_tv_update), 0, 1)) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

void
EditorRoutes::update_solo_safe_display ()
{
	if (g_atomic_int_compare_and_exchange (const_cast<gint*>(&_queue_tv_update), 0, 1)) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

list<TimeAxisView*>
EditorRoutes::views () const
{
	list<TimeAxisView*> v;
	for (TreeModel::Children::iterator i = _model->children().begin(); i != _model->children().end(); ++i) {
		v.push_back ((*i)[_columns.tv]);
	}

	return v;
}

void
EditorRoutes::clear ()
{
	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->clear ();
	_display.set_model (_model);
}

void
EditorRoutes::name_edit_started (CellEditable* ce, const Glib::ustring&)
{
	name_editable = ce;

	/* give it a special name */

	Gtk::Entry *e = dynamic_cast<Gtk::Entry*> (ce);

	if (e) {
		e->set_name (X_("RouteNameEditorEntry"));
	}
}

void
EditorRoutes::name_edit (std::string const & path, std::string const & new_text)
{
	name_editable = 0;

	TreeIter iter = _model->get_iter (path);

	if (!iter) {
		return;
	}

	boost::shared_ptr<Stripable> stripable = (*iter)[_columns.stripable];

	if (stripable && stripable->name() != new_text) {
		stripable->set_name (new_text);
	}
}

void
EditorRoutes::solo_changed_so_update_mute ()
{
	update_mute_display ();
}

void
EditorRoutes::show_tracks_with_regions_at_playhead ()
{
	boost::shared_ptr<RouteList> const r = _session->get_routes_with_regions_at (_session->transport_frame ());

	set<TimeAxisView*> show;
	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		TimeAxisView* tav = _editor->axis_view_from_stripable (*i);
		if (tav) {
			show.insert (tav);
		}
	}

	DisplaySuspender ds;

	TreeModel::Children rows = _model->children ();
	for (TreeModel::Children::iterator i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView* tv = (*i)[_columns.tv];
		bool to_show = (show.find (tv) != show.end());

		tv->set_marked_for_display (to_show);
		(*i)[_columns.visible] = to_show;
	}

	/* force route order keys catch up with visibility changes
	 */

	sync_presentation_info_from_treeview ();
}

int
EditorRoutes::plugin_setup (boost::shared_ptr<Route> r, boost::shared_ptr<PluginInsert> pi, ARDOUR::Route::PluginSetupOptions flags)
{
	PluginSetupDialog psd (r, pi, flags);
	return psd.run ();
}
