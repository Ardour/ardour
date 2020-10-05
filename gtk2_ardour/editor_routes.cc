/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2015 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
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
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/utils.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/cell_renderer_pixbuf_multi.h"
#include "gtkmm2ext/cell_renderer_pixbuf_toggle.h"
#include "gtkmm2ext/treeutils.h"

#include "widgets/tooltips.h"

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
#include "vca_time_axis.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ArdourWidgets;
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
	, _ignore_selection_change (false)
	, column_does_not_select (false)
	, _no_redisplay (false)
	, _adding_routes (false)
	, _route_deletion_in_progress (false)
	, _redisplay_on_resume (false)
	, _idle_update_queued (false)
	, _redisplay_active (0)
	, _menu (0)
	, old_focus (0)
	, name_editable (0)
{
	static const int column_width = 22;

	_scroller.add (_display);
	_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	_model = ListStore::create (_columns);
	_display.set_model (_model);

	_display.get_selection()->set_select_function (sigc::mem_fun (*this, &EditorRoutes::select_function));

	// Record enable toggle
	CellRendererPixbufMulti* rec_col_renderer = manage (new CellRendererPixbufMulti());

	rec_col_renderer->set_pixbuf (0, ::get_icon("record-normal-disabled"));
	rec_col_renderer->set_pixbuf (1, ::get_icon("record-normal-in-progress"));
	rec_col_renderer->set_pixbuf (2, ::get_icon("record-normal-enabled"));
	rec_col_renderer->set_pixbuf (3, ::get_icon("record-step"));
	rec_col_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_rec_enable_changed));

	rec_state_column = manage (new TreeViewColumn("R", *rec_col_renderer));

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

	rec_safe_column = manage (new TreeViewColumn(_("RS"), *rec_safe_renderer));
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

	input_active_column = manage (new TreeViewColumn ("I", *input_active_col_renderer));

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

	mute_state_column = manage (new TreeViewColumn("M", *mute_col_renderer));

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

	solo_state_column = manage (new TreeViewColumn("S", *solo_col_renderer));

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

	solo_isolate_state_column = manage (new TreeViewColumn("SI", *solo_iso_renderer));

	solo_isolate_state_column->add_attribute(solo_iso_renderer->property_state(), _columns.solo_isolate_state);
	solo_isolate_state_column->add_attribute(solo_iso_renderer->property_visible(), _columns.solo_lock_iso_visible);
	solo_isolate_state_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	solo_isolate_state_column->set_alignment(ALIGN_CENTER);
	solo_isolate_state_column->set_expand(false);
	solo_isolate_state_column->set_fixed_width(column_width);

	// Solo safe toggle
	CellRendererPixbufMulti* solo_safe_renderer = manage (new CellRendererPixbufMulti ());

	solo_safe_renderer->set_pixbuf (0, ::get_icon("solo-safe-disabled"));
	solo_safe_renderer->set_pixbuf (1, ::get_icon("solo-safe-enabled"));
	solo_safe_renderer->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::on_tv_solo_safe_toggled));

	solo_safe_state_column = manage (new TreeViewColumn(_("SS"), *solo_safe_renderer));
	solo_safe_state_column->add_attribute(solo_safe_renderer->property_state(), _columns.solo_safe_state);
	solo_safe_state_column->add_attribute(solo_safe_renderer->property_visible(), _columns.solo_lock_iso_visible);
	solo_safe_state_column->set_sizing(TREE_VIEW_COLUMN_FIXED);
	solo_safe_state_column->set_alignment(ALIGN_CENTER);
	solo_safe_state_column->set_expand(false);
	solo_safe_state_column->set_fixed_width(column_width);

	_name_column = _display.append_column ("", _columns.text) - 1;
	_visible_column = _display.append_column ("", _columns.visible) - 1;
	_active_column = _display.append_column ("", _columns.active) - 1;

	name_column = _display.get_column (_name_column);
	visible_column = _display.get_column (_visible_column);
	active_column = _display.get_column (_active_column);

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
	_display.get_selection()->set_mode (SELECTION_MULTIPLE);
	_display.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &EditorRoutes::selection_changed));
	_display.set_reorderable (true);
	_display.set_name (X_("EditGroupList"));
	_display.set_rules_hint (true);
	_display.set_size_request (100, -1);

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
	active_col->add_attribute (active_cell->property_visible(), _columns.no_vca);

	_model->signal_row_deleted().connect (sigc::mem_fun (*this, &EditorRoutes::row_deleted));
	_model->signal_rows_reordered().connect (sigc::mem_fun (*this, &EditorRoutes::reordered));

	_display.signal_button_press_event().connect (sigc::mem_fun (*this, &EditorRoutes::button_press), false);
	_display.signal_button_release_event().connect (sigc::mem_fun (*this, &EditorRoutes::button_release), false);
	_scroller.signal_key_press_event().connect (sigc::mem_fun(*this, &EditorRoutes::key_press), false);

	_scroller.signal_focus_in_event().connect (sigc::mem_fun (*this, &EditorRoutes::focus_in), false);
	_scroller.signal_focus_out_event().connect (sigc::mem_fun (*this, &EditorRoutes::focus_out));

	_display.signal_enter_notify_event().connect (sigc::mem_fun (*this, &EditorRoutes::enter_notify), false);
	_display.signal_leave_notify_event().connect (sigc::mem_fun (*this, &EditorRoutes::leave_notify), false);

	_display.set_enable_search (false);

	Route::PluginSetup.connect_same_thread (*this, boost::bind (&EditorRoutes::plugin_setup, this, _1, _2, _3));
}

EditorRoutes::~EditorRoutes ()
{
	delete _menu;
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

	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
EditorRoutes::leave_notify (GdkEventCrossing*)
{
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
	StripableTimeAxisView* stv = dynamic_cast<StripableTimeAxisView*> (tv);

	if (!stv || !stv->stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac = stv->stripable()->rec_enable_control();

	if (ac) {
		ac->set_value (!ac->get_value(), Controllable::UseGroup);
	}
}

void
EditorRoutes::on_tv_rec_safe_toggled (std::string const & path_string)
{
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));
	TimeAxisView* tv = row[_columns.tv];
	StripableTimeAxisView* stv = dynamic_cast<StripableTimeAxisView*> (tv);

	if (!stv || !stv->stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (stv->stripable()->rec_safe_control());

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
	StripableTimeAxisView* stv = dynamic_cast<StripableTimeAxisView*> (tv);

	if (!stv || !stv->stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (stv->stripable()->mute_control());

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
	StripableTimeAxisView* stv = dynamic_cast<StripableTimeAxisView*> (tv);

	if (!stv || !stv->stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (stv->stripable()->solo_control());

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
	StripableTimeAxisView* stv = dynamic_cast<StripableTimeAxisView*> (tv);

	if (!stv || !stv->stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (stv->stripable()->solo_isolate_control());

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
	StripableTimeAxisView* stv = dynamic_cast<StripableTimeAxisView*> (tv);

	if (!stv || !stv->stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac (stv->stripable()->solo_safe_control());

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
	items.push_back (MenuElem (_("Show All Midi Tracks"), sigc::mem_fun (*this, &EditorRoutes::show_all_miditracks)));
	items.push_back (MenuElem (_("Hide All Midi Tracks"), sigc::mem_fun (*this, &EditorRoutes::hide_all_miditracks)));
	items.push_back (MenuElem (_("Show All Busses"), sigc::mem_fun (*this, &EditorRoutes::show_all_audiobus)));
	items.push_back (MenuElem (_("Hide All Busses"), sigc::mem_fun (*this, &EditorRoutes::hide_all_audiobus)));
	items.push_back (MenuElem (_("Only Show Tracks with Regions Under Playhead"), sigc::mem_fun (*this, &EditorRoutes::show_tracks_with_regions_at_playhead)));
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

	++_redisplay_active;
	if (_redisplay_active != 1) {
		/* recursive re-display can happen if redisplay shows/hides a TrackView
		 * which has children and their display status changes as result.
		 */
		return;
	}

	redisplay_real ();

	while (_redisplay_active != 1) {
		_redisplay_active = 1;
		redisplay_real ();
	}
	_redisplay_active = 0;
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

	{
		PBD::Unwinder<bool> uw (_ignore_selection_change, true);
		_display.set_model (Glib::RefPtr<ListStore>());
	}

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
			row[_columns.no_vca] = false;

		} else if (rtav) {

			stripable = rtav->route ();
			midi_trk= boost::dynamic_pointer_cast<MidiTrack> (stripable);

			row[_columns.is_track] = (boost::dynamic_pointer_cast<Track> (stripable) != 0);
			row[_columns.no_vca] = true;

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
		row[_columns.solo_visible] = !stripable->is_master ();
		row[_columns.solo_lock_iso_visible] = row[_columns.solo_visible] && row[_columns.no_vca];
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

	{
		PBD::Unwinder<bool> uw (_ignore_selection_change, true);
		_display.set_model (_model);
	}

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

	PBD::Unwinder<bool> uw (_ignore_selection_change, true);

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[_columns.tv] == tv) {
			PBD::Unwinder<bool> uw (_route_deletion_in_progress, true);
			_model->erase (ri);
			break;
		}
	}
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
				redisplay ();

			}

			break;
		}
	}
}

void
EditorRoutes::update_active_display ()
{
	if (!_idle_update_queued) {
		_idle_update_queued = true;
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

	/* force route order keys catch up with visibility changes */

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

	bool change = false;
	PresentationInfo::order_t order = 0;

	PresentationInfo::ChangeSuspender cs;

	for (TreeModel::Children::iterator ri = rows.begin(); ri != rows.end(); ++ri) {
		boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
		bool visible = (*ri)[_columns.visible];

#ifndef NDEBUG // these should not exist in the treeview
		assert (stripable);
		if (stripable->is_monitor() || stripable->is_auditioner()) {
			assert (0);
			continue;
		}
#endif

		stripable->presentation_info().set_hidden (!visible);

		if (order != stripable->presentation_info().order()) {
			stripable->set_presentation_order (order);
			change = true;
		}
		++order;
	}

	change |= _session->ensure_stripable_sort_order ();

	if (change) {
		_session->set_dirty();
	}
}

void
EditorRoutes::sync_treeview_from_presentation_info (PropertyChange const & what_changed)
{
	/* Some route order key(s) have been changed, make sure that
	   we update out tree/list model and GUI to reflect the change.
	*/

	if (_ignore_reorder || !_session || _session->deletion_in_progress()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "editor sync model from presentation info.\n");

	PropertyChange hidden_or_order;
	hidden_or_order.add (Properties::hidden);
	hidden_or_order.add (Properties::order);

	TreeModel::Children rows = _model->children();

	bool changed = false;

	if (what_changed.contains (hidden_or_order)) {
		vector<int> neworder;
		uint32_t old_order = 0;

		if (rows.empty()) {
			return;
		}

		TreeOrderKeys sorted;
		for (TreeModel::Children::iterator ri = rows.begin(); ri != rows.end(); ++ri, ++old_order) {
			boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
			/* use global order */
			sorted.push_back (TreeOrderKey (old_order, stripable));
		}

		TreeOrderKeySorter cmp;

		sort (sorted.begin(), sorted.end(), cmp);
		neworder.assign (sorted.size(), 0);

		uint32_t n = 0;

		for (TreeOrderKeys::iterator sr = sorted.begin(); sr != sorted.end(); ++sr, ++n) {

			neworder[n] = sr->old_display_order;

			if (sr->old_display_order != n) {
				changed = true;
			}
		}

		if (changed) {
			Unwinder<bool> uw (_ignore_reorder, true);
			/* prevent traverse_cells: assertion 'row_path != NULL'
			 * in case of DnD re-order: row-removed + row-inserted.
			 *
			 * The rows (stripables) are not actually removed from the model,
			 * but only from the display in the DnDTreeView.
			 * ->reorder() will fail to find the row_path.
			 * (re-order drag -> remove row -> sync PI from TV -> notify -> sync TV from PI -> crash)
			 */
			Unwinder<bool> uw2 (_ignore_selection_change, true);

			_display.unset_model();
			_model->reorder (neworder);
			_display.set_model (_model);
		}
	}

	if (changed || what_changed.contains (Properties::selected)) {
		/* by the time this is invoked, the GUI Selection model has
		 * already updated itself.
		 */
		PBD::Unwinder<bool> uw (_ignore_selection_change, true);

		/* set the treeview model selection state */
		for (TreeModel::Children::iterator ri = rows.begin(); ri != rows.end(); ++ri) {
			boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
			if (stripable && stripable->is_selected()) {
				_display.get_selection()->select (*ri);
			} else {
				_display.get_selection()->unselect (*ri);
			}
		}
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
EditorRoutes::select_function(const Glib::RefPtr<Gtk::TreeModel>&, const Gtk::TreeModel::Path&, bool)
{
	return !column_does_not_select;
}

bool
EditorRoutes::button_release (GdkEventButton*)
{
	column_does_not_select = false;
	return false;
}

bool
EditorRoutes::button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		if (_menu == 0) {
			build_menu ();
		}
		_menu->popup (ev->button, ev->time);
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

	if ((tvc == rec_state_column) ||
	    (tvc == rec_safe_column) ||
	    (tvc == input_active_column) ||
	    (tvc == mute_state_column) ||
	    (tvc == solo_state_column) ||
	    (tvc == solo_safe_state_column) ||
	    (tvc == solo_isolate_state_column) ||
	    (tvc == visible_column) ||
	    (tvc == active_column)) {
		column_does_not_select = true;
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
	if (_ignore_selection_change || column_does_not_select) {
		return;
	}

	_editor->begin_reversible_selection_op (X_("Select Track from Route List"));

	if (_display.get_selection()->count_selected_rows() > 0) {

		TreeIter iter;
		TreeView::Selection::ListHandle_Path rows = _display.get_selection()->get_selected_rows ();
		TrackViewList selected;

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

void
EditorRoutes::initial_display ()
{

	if (!_session) {
		clear ();
		return;
	}

	DisplaySuspender ds;
	_model->clear ();

	StripableList s;

	_session->get_stripables (s);
	_editor->add_stripables (s);

	sync_treeview_from_presentation_info (Properties::order);
}

struct ViewStripable {
	TimeAxisView* tav;
	boost::shared_ptr<Stripable> stripable;

	ViewStripable (TimeAxisView* t, boost::shared_ptr<Stripable> s)
		: tav (t), stripable (s) {}
};

void
EditorRoutes::move_selected_tracks (bool up)
{
	TimeAxisView* scroll_to = 0;
	StripableList sl;
	_session->get_stripables (sl);

	if (sl.size() < 2) {
		/* nope */
		return;
	}

	sl.sort (Stripable::Sorter());

	std::list<ViewStripable> view_stripables;

	/* build a list that includes time axis view information */

	for (StripableList::const_iterator sli = sl.begin(); sli != sl.end(); ++sli) {
		TimeAxisView* tv = _editor->time_axis_view_from_stripable (*sli);
		view_stripables.push_back (ViewStripable (tv, *sli));
	}

	/* for each selected stripable, move it above or below the adjacent
	 * stripable that has a time-axis view representation here. If there's
	 * no such representation, then
	 */

	list<ViewStripable>::iterator unselected_neighbour;
	list<ViewStripable>::iterator vsi;

	{
		PresentationInfo::ChangeSuspender cs;

		if (up) {
			unselected_neighbour = view_stripables.end ();
			vsi = view_stripables.begin();

			while (vsi != view_stripables.end()) {

				if (vsi->stripable->is_selected()) {

					if (unselected_neighbour != view_stripables.end()) {

						PresentationInfo::order_t unselected_neighbour_order = unselected_neighbour->stripable->presentation_info().order();
						PresentationInfo::order_t my_order = vsi->stripable->presentation_info().order();

						unselected_neighbour->stripable->set_presentation_order (my_order);
						vsi->stripable->set_presentation_order (unselected_neighbour_order);

						if (!scroll_to) {
							scroll_to = vsi->tav;
						}
					}

				} else {

					if (vsi->tav) {
						unselected_neighbour = vsi;
					}

				}

				++vsi;
			}

		} else {

			unselected_neighbour = view_stripables.end();
			vsi = unselected_neighbour;

			do {

				--vsi;

				if (vsi->stripable->is_selected()) {

					if (unselected_neighbour != view_stripables.end()) {

						PresentationInfo::order_t unselected_neighbour_order = unselected_neighbour->stripable->presentation_info().order();
						PresentationInfo::order_t my_order = vsi->stripable->presentation_info().order();

						unselected_neighbour->stripable->set_presentation_order (my_order);
						vsi->stripable->set_presentation_order (unselected_neighbour_order);

						if (!scroll_to) {
							scroll_to = vsi->tav;
						}
					}

				} else {

					if (vsi->tav) {
						unselected_neighbour = vsi;
					}

				}

			} while (vsi != view_stripables.begin());
		}
	}

	if (scroll_to) {
		_editor->ensure_time_axis_view_is_visible (*scroll_to, false);
	}
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
	if (!_idle_update_queued) {
		_idle_update_queued = true;
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

bool
EditorRoutes::idle_update_mute_rec_solo_etc()
{
	_idle_update_queued = false;
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
	if (!_idle_update_queued) {
		_idle_update_queued = true;
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

void
EditorRoutes::update_solo_display ()
{
	if (!_idle_update_queued) {
		_idle_update_queued = true;
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

void
EditorRoutes::update_solo_isolate_display ()
{
	if (!_idle_update_queued) {
		_idle_update_queued = true;
		Glib::signal_idle().connect (sigc::mem_fun (*this, &EditorRoutes::idle_update_mute_rec_solo_etc));
	}
}

void
EditorRoutes::update_solo_safe_display ()
{
	if (!_idle_update_queued) {
		_idle_update_queued = true;
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
	PBD::Unwinder<bool> uw (_ignore_selection_change, true);
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
	boost::shared_ptr<RouteList> const r = _session->get_routes_with_regions_at (timepos_t (_session->transport_sample ()));

	set<TimeAxisView*> show;
	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		TimeAxisView* tav = _editor->time_axis_view_from_stripable (*i);
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
	int rv = psd.run ();
	return rv + (psd.fan_out() ? 4 : 0);
}
