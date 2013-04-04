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

#include <list>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cassert>

#include "pbd/unknown_type.h"
#include "pbd/unwind.h"

#include "ardour/debug.h"
#include "ardour/route.h"
#include "ardour/midi_track.h"
#include "ardour/session.h"

#include "gtkmm2ext/cell_renderer_pixbuf_multi.h"
#include "gtkmm2ext/cell_renderer_pixbuf_toggle.h"
#include "gtkmm2ext/treeutils.h"

#include "editor.h"
#include "keyboard.h"
#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"
#include "mixer_strip.h"
#include "gui_thread.h"
#include "actions.h"
#include "utils.h"
#include "route_sorter.h"
#include "editor_group_tabs.h"
#include "editor_routes.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
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
	_display.append_column (*mute_state_column);
	_display.append_column (*solo_state_column);
	_display.append_column (*solo_isolate_state_column);
	_display.append_column (*solo_safe_state_column);


	TreeViewColumn* col;
	Gtk::Label* l;

	ColumnInfo ci[] = {
		{ 0, _("Name"), _("Track/Bus Name") },
		{ 1, _("V"), _("Track/Bus visible ?") },
		{ 2, _("A"), _("Track/Bus active ?") },
		{ 3, _("I"), _("MIDI input enabled") },
		{ 4, _("R"), _("Record enabled") },
		{ 5, _("M"), _("Muted") },
		{ 6, _("S"), _("Soloed") },
		{ 7, _("SI"), _("Solo Isolated") },
		{ 8, _("SS"), _("Solo Safe (Locked)") },
		{ -1, 0, 0 }
	};

	for (int i = 0; ci[i].index >= 0; ++i) {
		col = _display.get_column (ci[i].index);
		l = manage (new Label (ci[i].label));
		ARDOUR_UI::instance()->set_tip (*l, ci[i].tooltip);
		col->set_widget (*l);
		l->show ();
	}

	_display.set_headers_visible (true);
	_display.get_selection()->set_mode (SELECTION_SINGLE);
	_display.get_selection()->set_select_function (sigc::mem_fun (*this, &EditorRoutes::selection_filter));
	_display.set_reorderable (true);
	_display.set_name (X_("EditGroupList"));
	_display.set_rules_hint (true);
	_display.set_size_request (100, -1);
	_display.add_object_drag (_columns.route.index(), "routes");

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
	
	_model->signal_row_deleted().connect (sigc::mem_fun (*this, &EditorRoutes::route_deleted));
	_model->signal_rows_reordered().connect (sigc::mem_fun (*this, &EditorRoutes::reordered));

	_display.signal_button_press_event().connect (sigc::mem_fun (*this, &EditorRoutes::button_press), false);
	_scroller.signal_key_press_event().connect (sigc::mem_fun(*this, &EditorRoutes::key_press), false);

        _scroller.signal_focus_in_event().connect (sigc::mem_fun (*this, &EditorRoutes::focus_in), false);
        _scroller.signal_focus_out_event().connect (sigc::mem_fun (*this, &EditorRoutes::focus_out));

        _display.signal_enter_notify_event().connect (sigc::mem_fun (*this, &EditorRoutes::enter_notify), false);
        _display.signal_leave_notify_event().connect (sigc::mem_fun (*this, &EditorRoutes::leave_notify), false);

        _display.set_enable_search (false);

	Route::SyncOrderKeys.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::sync_treeview_from_order_keys, this, _1), gui_context());
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
           next two attempts to change selection status.
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

	if (rtv && rtv->track()) {
		boost::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (rtv->route());
		_session->set_record_enabled (rl, !rtv->track()->record_enabled(), Session::rt_cleanup);
	}
}

void
EditorRoutes::on_tv_mute_enable_toggled (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	RouteTimeAxisView *rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (rtv != 0) {
		boost::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (rtv->route());
		_session->set_mute (rl, !rtv->route()->muted(), Session::rt_cleanup);
	}
}

void
EditorRoutes::on_tv_solo_enable_toggled (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (rtv != 0) {
		boost::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (rtv->route());
		if (Config->get_solo_control_is_listen_control()) {
			_session->set_listen (rl, !rtv->route()->listening_via_monitor(), Session::rt_cleanup);
		} else {
			_session->set_solo (rl, !rtv->route()->self_soloed(), Session::rt_cleanup);
		}
	}
}

void
EditorRoutes::on_tv_solo_isolate_toggled (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (rtv) {
		rtv->route()->set_solo_isolated (!rtv->route()->solo_isolated(), this);
	}
}

void
EditorRoutes::on_tv_solo_safe_toggled (std::string const & path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row row = *_model->get_iter (Gtk::TreeModel::Path (path_string));

	TimeAxisView *tv = row[_columns.tv];
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);

	if (rtv) {
		rtv->route()->set_solo_safe (!rtv->route()->solo_safe(), this);
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
	items.push_back (MenuElem (_("Show Tracks With Regions Under Playhead"), sigc::mem_fun (*this, &EditorRoutes::show_tracks_with_regions_at_playhead)));
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
EditorRoutes::redisplay ()
{
	if (_no_redisplay || !_session || _session->deletion_in_progress()) {
		return;
	}

	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;
	uint32_t position;

	/* n will be the count of tracks plus children (updated by TimeAxisView::show_at),
	   so we will use that to know where to put things.
	*/
	int n;

	for (n = 0, position = 0, i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[_columns.tv];
		boost::shared_ptr<Route> route = (*i)[_columns.route];

		if (tv == 0) {
			// just a "title" row
			continue;
		}

		bool visible = tv->marked_for_display ();

		/* show or hide the TimeAxisView */
		if (visible) {
			position += tv->show_at (position, n, &_editor->edit_controls_vbox);
			tv->clip_to_viewport ();
		} else {
			tv->hide ();
		}

		n++;
	}

	/* whenever we go idle, update the track view list to reflect the new order.
	   we can't do this here, because we could mess up something that is traversing
	   the track order and has caused a redisplay of the list.
	*/
	Glib::signal_idle().connect (sigc::mem_fun (*_editor, &Editor::sync_track_view_list_and_routes));

        _editor->reset_controls_layout_height (position);
        _editor->reset_controls_layout_width ();
	_editor->_full_canvas_height = position;
	_editor->vertical_adjustment.set_upper (_editor->_full_canvas_height);

	if ((_editor->vertical_adjustment.get_value() + _editor->_visible_canvas_height) > _editor->vertical_adjustment.get_upper()) {
		/*
		   We're increasing the size of the canvas while the bottom is visible.
		   We scroll down to keep in step with the controls layout.
		*/
		_editor->vertical_adjustment.set_value (_editor->_full_canvas_height - _editor->_visible_canvas_height);
	}
}

void
EditorRoutes::route_deleted (Gtk::TreeModel::Path const &)
{
	/* this happens as the second step of a DnD within the treeview as well
	   as when a row/route is actually deleted.
	*/
	DEBUG_TRACE (DEBUG::OrderKeys, "editor routes treeview row deleted\n");
	sync_order_keys_from_treeview ();
}

void
EditorRoutes::reordered (TreeModel::Path const &, TreeModel::iterator const &, int* /*what*/)
{
	DEBUG_TRACE (DEBUG::OrderKeys, "editor routes treeview reordered\n");
	sync_order_keys_from_treeview ();
}

void
EditorRoutes::visible_changed (std::string const & path)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

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
	boost::shared_ptr<Route> route = row[_columns.route];
	bool const active = row[_columns.active];
	route->set_active (!active, this);
}

void
EditorRoutes::routes_added (list<RouteTimeAxisView*> routes)
{
	TreeModel::Row row;
	PBD::Unwinder<bool> at (_adding_routes, true);

	suspend_redisplay ();

	_display.set_model (Glib::RefPtr<ListStore>());

	for (list<RouteTimeAxisView*>::iterator x = routes.begin(); x != routes.end(); ++x) {

		boost::shared_ptr<MidiTrack> midi_trk = boost::dynamic_pointer_cast<MidiTrack> ((*x)->route());

		row = *(_model->append ());

		row[_columns.text] = (*x)->route()->name();
		row[_columns.visible] = (*x)->marked_for_display();
		row[_columns.active] = (*x)->route()->active ();
		row[_columns.tv] = *x;
		row[_columns.route] = (*x)->route ();
		row[_columns.is_track] = (boost::dynamic_pointer_cast<Track> ((*x)->route()) != 0);

		if (midi_trk) {
			row[_columns.is_input_active] = midi_trk->input_active ();
			row[_columns.is_midi] = true;
		} else {
			row[_columns.is_input_active] = false;
			row[_columns.is_midi] = false;
		}

		row[_columns.mute_state] = (*x)->route()->muted() ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off;
		row[_columns.solo_state] = RouteUI::solo_active_state ((*x)->route());
		row[_columns.solo_visible] = !(*x)->route()->is_master ();
		row[_columns.solo_isolate_state] = (*x)->route()->solo_isolated();
		row[_columns.solo_safe_state] = (*x)->route()->solo_safe();
		row[_columns.name_editable] = true;

		boost::weak_ptr<Route> wr ((*x)->route());

		(*x)->route()->gui_changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::handle_gui_changes, this, _1, _2), gui_context());
		(*x)->route()->PropertyChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::route_property_changed, this, _1, wr), gui_context());

		if ((*x)->is_track()) {
			boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> ((*x)->route());
			t->RecordEnableChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_rec_display, this), gui_context());
		}

		if ((*x)->is_midi_track()) {
			boost::shared_ptr<MidiTrack> t = boost::dynamic_pointer_cast<MidiTrack> ((*x)->route());
			t->StepEditStatusChange.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_rec_display, this), gui_context());
			t->InputActiveChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_input_active_display, this), gui_context());
		}

		(*x)->route()->mute_changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_mute_display, this), gui_context());
		(*x)->route()->solo_changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_solo_display, this, _1), gui_context());
		(*x)->route()->listen_changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_solo_display, this, _1), gui_context());
		(*x)->route()->solo_isolated_changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_solo_isolate_display, this), gui_context());
		(*x)->route()->solo_safe_changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_solo_safe_display, this), gui_context());
		(*x)->route()->active_changed.connect (*this, MISSING_INVALIDATOR, boost::bind (&EditorRoutes::update_active_display, this), gui_context ());

	}

	update_rec_display ();
	update_mute_display ();
	update_solo_display (true);
	update_solo_isolate_display ();
	update_solo_safe_display ();
	update_input_active_display ();
	update_active_display ();

	resume_redisplay ();
	_display.set_model (_model);

	/* now update route order keys from the treeview/track display order */

	sync_order_keys_from_treeview ();
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
		//update_canvas_now ();
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
			_model->erase (ri);
			break;
		}
	}

	/* the deleted signal for the treeview/model will take 
	   care of any updates.
	*/
}

void
EditorRoutes::route_property_changed (const PropertyChange& what_changed, boost::weak_ptr<Route> r)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &EditorRoutes::route_name_changed, r)

	boost::shared_ptr<Route> route = r.lock ();

	if (!route) {
		return;
	}

	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> t = (*i)[_columns.route];
		if (t == route) {
			(*i)[_columns.text] = route->name();
			break;
		}
	}
}

void
EditorRoutes::update_active_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];
		(*i)[_columns.active] = route->active ();
	}
}

void
EditorRoutes::update_visibility ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	suspend_redisplay ();

	for (i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView *tv = (*i)[_columns.tv];
		(*i)[_columns.visible] = tv->marked_for_display ();
	}

	/* force route order keys catch up with visibility changes
	 */

	sync_order_keys_from_treeview ();

	resume_redisplay ();
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
EditorRoutes::reset_remote_control_ids ()
{
	if (Config->get_remote_model() != EditorOrdered || !_session || _session->deletion_in_progress()) {
		return;
	}

	TreeModel::Children rows = _model->children();
	
	if (rows.empty()) {
		return;
	}

	
	DEBUG_TRACE (DEBUG::OrderKeys, "editor reset remote control ids\n");

	TreeModel::Children::iterator ri;
	bool rid_change = false;
	uint32_t rid = 1;
	uint32_t invisible_key = UINT32_MAX;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {

		boost::shared_ptr<Route> route = (*ri)[_columns.route];
		bool visible = (*ri)[_columns.visible];


		if (!route->is_master() && !route->is_monitor()) {

			uint32_t new_rid = (visible ? rid : invisible_key--);

			if (new_rid != route->remote_control_id()) {
				route->set_remote_control_id_from_order_key (EditorSort, new_rid);	
				rid_change = true;
			}
			
			if (visible) {
				rid++;
			}

		}
	}

	if (rid_change) {
		/* tell the world that we changed the remote control IDs */
		_session->notify_remote_id_change ();
	}
}


void
EditorRoutes::sync_order_keys_from_treeview ()
{
	if (_ignore_reorder || !_session || _session->deletion_in_progress()) {
		return;
	}

	TreeModel::Children rows = _model->children();
	
	if (rows.empty()) {
		return;
	}

	
	DEBUG_TRACE (DEBUG::OrderKeys, "editor sync order keys from treeview\n");

	TreeModel::Children::iterator ri;
	bool changed = false;
	bool rid_change = false;
	uint32_t order = 0;
	uint32_t rid = 1;
	uint32_t invisible_key = UINT32_MAX;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {

		boost::shared_ptr<Route> route = (*ri)[_columns.route];
		bool visible = (*ri)[_columns.visible];

		uint32_t old_key = route->order_key (EditorSort);

		if (order != old_key) {
			route->set_order_key (EditorSort, order);

			changed = true;
		}

		if ((Config->get_remote_model() == EditorOrdered) && !route->is_master() && !route->is_monitor()) {

			uint32_t new_rid = (visible ? rid : invisible_key--);

			if (new_rid != route->remote_control_id()) {
				route->set_remote_control_id_from_order_key (EditorSort, new_rid);	
				rid_change = true;
			}
			
			if (visible) {
				rid++;
			}

		}

		++order;
	}
	
	if (changed) {
		/* tell the world that we changed the editor sort keys */
		_session->sync_order_keys (EditorSort);
	}

	if (rid_change) {
		/* tell the world that we changed the remote control IDs */
		_session->notify_remote_id_change ();
	}
}

void
EditorRoutes::sync_treeview_from_order_keys (RouteSortOrderKey src)
{
	/* Some route order key(s) for `src' has been changed, make sure that 
	   we update out tree/list model and GUI to reflect the change.
	*/

	if (!_session || _session->deletion_in_progress()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("editor sync model from order keys, src = %1\n", enum_2_string (src)));

	if (src == MixerSort) {

		if (!Config->get_sync_all_route_ordering()) {
			/* mixer sort keys changed - we don't care */
			return;
		}

		DEBUG_TRACE (DEBUG::OrderKeys, "reset editor order key to match mixer\n");

		/* mixer sort keys were changed, update the editor sort
		 * keys since "sync mixer+editor order" is enabled.
		 */

		boost::shared_ptr<RouteList> r = _session->get_routes ();
		
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->sync_order_keys (src);
		}
	}

	/* we could get here after either a change in the Mixer or Editor sort
	 * order, but either way, the mixer order keys reflect the intended
	 * order for the GUI, so reorder the treeview model to match it.
	 */

	vector<int> neworder;
	TreeModel::Children rows = _model->children();
	uint32_t old_order = 0;
	bool changed = false;

	if (rows.empty()) {
		return;
	}

	OrderKeySortedRoutes sorted_routes;

	for (TreeModel::Children::iterator ri = rows.begin(); ri != rows.end(); ++ri, ++old_order) {
		boost::shared_ptr<Route> route = (*ri)[_columns.route];
		sorted_routes.push_back (RoutePlusOrderKey (route, old_order, route->order_key (EditorSort)));
	}

	SortByNewDisplayOrder cmp;

	sort (sorted_routes.begin(), sorted_routes.end(), cmp);
	neworder.assign (sorted_routes.size(), 0);

	uint32_t n = 0;
	
	for (OrderKeySortedRoutes::iterator sr = sorted_routes.begin(); sr != sorted_routes.end(); ++sr, ++n) {

		neworder[n] = sr->old_display_order;

		if (sr->old_display_order != n) {
			changed = true;
		}

		DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("EDITOR change order for %1 from %2 to %3\n",
							       sr->route->name(), sr->old_display_order, n));
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

	suspend_redisplay ();

	for (i = rows.begin(); i != rows.end(); ++i) {

		TreeModel::Row row = (*i);
		TimeAxisView *tv = row[_columns.tv];

		if (tv == 0) {
			continue;
		}

		row[_columns.visible] = false;
	}

	resume_redisplay ();

	/* XXX this seems like a hack and half, but its not clear where to put this
	   otherwise.
	*/

	//reset_scrolling_region ();
}

void
EditorRoutes::set_all_tracks_visibility (bool yn)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	suspend_redisplay ();

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

	sync_order_keys_from_treeview ();

	resume_redisplay ();
}

void
EditorRoutes::set_all_audio_midi_visibility (int tracks, bool yn)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	suspend_redisplay ();

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
				(*i)[_columns.visible] = yn;
				break;

			case 1:
				if (atv->is_audio_track()) {
					(*i)[_columns.visible] = yn;
				}
				break;

			case 2:
				if (!atv->is_audio_track()) {
					(*i)[_columns.visible] = yn;
				}
				break;
			}
		}
		else if ((mtv = dynamic_cast<MidiTimeAxisView*>(tv)) != 0) {
			switch (tracks) {
			case 0:
				(*i)[_columns.visible] = yn;
				break;

			case 3:
				if (mtv->is_midi_track()) {
					(*i)[_columns.visible] = yn;
				}
				break;
			}
		}
	}

	/* force route order keys catch up with visibility changes
	 */

	sync_order_keys_from_treeview ();

	resume_redisplay ();
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

                /* If we appear to be editing something, leave that cleanly and appropriately.
                */
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
                        _session->set_mute (rl, !rl->front()->muted(), Session::rt_cleanup);
                }
                return true;
                break;

        case 's':
                if (get_relevant_routes (rl)) {
			if (Config->get_solo_control_is_listen_control()) {
				_session->set_listen (rl, !rl->front()->listening_via_monitor(), Session::rt_cleanup);
			} else {
				_session->set_solo (rl, !rl->front()->self_soloed(), Session::rt_cleanup);
			}
		}
                return true;
                break;

        case 'r':
                if (get_relevant_routes (rl)) {
                        _session->set_record_enabled (rl, !rl->front()->record_enabled(), Session::rt_cleanup);
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

		// Get the model row.
		Gtk::TreeModel::Row row = *_model->get_iter (path);

		TimeAxisView *tv = row[_columns.tv];

		int y_pos = tv->y_position();

		//Clamp the y pos so that we do not extend beyond the canvas full height.
		if (_editor->_full_canvas_height - y_pos < _editor->_visible_canvas_height){
		    y_pos = _editor->_full_canvas_height - _editor->_visible_canvas_height;
		}

		//Only scroll to if the track is visible
		if(y_pos != -1){
		    _editor->reset_y_origin (y_pos);
		}
	}

	return false;
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

struct EditorOrderRouteSorter {
    bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
	    if (a->is_master()) {
		    /* master before everything else */
		    return true;
	    } else if (b->is_master()) {
		    /* everything else before master */
		    return false;
	    }
	    return a->order_key (EditorSort) < b->order_key (EditorSort);
    }
};

void
EditorRoutes::initial_display ()
{
	suspend_redisplay ();
	_model->clear ();

	if (!_session) {
		resume_redisplay ();
		return;
	}

	boost::shared_ptr<RouteList> routes = _session->get_routes();

	if (ARDOUR_UI::instance()->session_is_new ()) {

		/* new session: stamp all routes with the right editor order
		 * key
		 */

		_editor->add_routes (*(routes.get()));
		
	} else {

		/* existing session: sort a copy of the route list by
		 * editor-order and add its contents to the display.
		 */

		RouteList r (*routes);
		EditorOrderRouteSorter sorter;
		
		r.sort (sorter);
		_editor->add_routes (r);
		
	}

	resume_redisplay ();
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

void
EditorRoutes::move_selected_tracks (bool up)
{
	if (_editor->selection->tracks.empty()) {
		return;
	}

	typedef std::pair<TimeAxisView*,boost::shared_ptr<Route> > ViewRoute;
	std::list<ViewRoute> view_routes;
	std::vector<int> neworder;
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator ri;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		TimeAxisView* tv = (*ri)[_columns.tv];
		boost::shared_ptr<Route> route = (*ri)[_columns.route];

		view_routes.push_back (ViewRoute (tv, route));
	}

	list<ViewRoute>::iterator trailing;
	list<ViewRoute>::iterator leading;

	if (up) {

		trailing = view_routes.begin();
		leading = view_routes.begin();

		++leading;

		while (leading != view_routes.end()) {
			if (_editor->selection->selected (leading->first)) {
				view_routes.insert (trailing, ViewRoute (leading->first, leading->second));
				leading = view_routes.erase (leading);
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

		trailing = view_routes.end();
		leading = view_routes.end();

		--leading; if (leading == view_routes.begin()) { return; }
		--leading;
		--trailing;

		while (1) {

			if (_editor->selection->selected (leading->first)) {
				list<ViewRoute>::iterator tmp;

				/* need to insert *after* trailing, not *before* it,
				   which is what insert (iter, val) normally does.
				*/

				tmp = trailing;
				tmp++;

				view_routes.insert (tmp, ViewRoute (leading->first, leading->second));

				/* can't use iter = cont.erase (iter); form here, because
				   we need iter to move backwards.
				*/

				tmp = leading;
				--tmp;

				bool done = false;

				if (leading == view_routes.begin()) {
					/* the one we've just inserted somewhere else
					   was the first in the list. erase this copy,
					   and then break, because we're done.
					*/
					done = true;
				}

				view_routes.erase (leading);

				if (done) {
					break;
				}

				leading = tmp;

			} else {
				if (leading == view_routes.begin()) {
					break;
				}
				--leading;
				--trailing;
			}
		};
	}

	for (leading = view_routes.begin(); leading != view_routes.end(); ++leading) {
		uint32_t order = leading->second->order_key (EditorSort);
		neworder.push_back (order);
	}

#ifndef NDEBUG
	DEBUG_TRACE (DEBUG::OrderKeys, "New order after moving tracks:\n");
	for (vector<int>::iterator i = neworder.begin(); i != neworder.end(); ++i) {
		DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("\t%1\n", *i));
	}
	DEBUG_TRACE (DEBUG::OrderKeys, "-------\n");

	for (vector<int>::iterator i = neworder.begin(); i != neworder.end(); ++i) {
		if (*i >= (int) neworder.size()) {
			cerr << "Trying to move something to " << *i << " of " << neworder.size() << endl;
		}
		assert (*i < (int) neworder.size ());
	}
#endif	

	_model->reorder (neworder);
}

void
EditorRoutes::update_input_active_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];

		if (boost::dynamic_pointer_cast<Track> (route)) {
			boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (route);
			
			if (mt) {
				(*i)[_columns.is_input_active] = mt->input_active();
			}
		}
	}
}

void
EditorRoutes::update_rec_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];

		if (boost::dynamic_pointer_cast<Track> (route)) {
			boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (route);

			if (route->record_enabled()) {
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

			(*i)[_columns.name_editable] = !route->record_enabled ();
		}
	}
}

void
EditorRoutes::update_mute_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];
		(*i)[_columns.mute_state] = RouteUI::mute_active_state (_session, route);
	}
}

void
EditorRoutes::update_solo_display (bool /* selfsoloed */)
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];
		(*i)[_columns.solo_state] = RouteUI::solo_active_state (route);
	}
}

void
EditorRoutes::update_solo_isolate_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];
		(*i)[_columns.solo_isolate_state] = RouteUI::solo_isolate_active_state (route) ? 1 : 0;
	}
}

void
EditorRoutes::update_solo_safe_display ()
{
	TreeModel::Children rows = _model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Route> route = (*i)[_columns.route];
		(*i)[_columns.solo_safe_state] = RouteUI::solo_safe_active_state (route) ? 1 : 0;
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

	boost::shared_ptr<Route> route = (*iter)[_columns.route];

	if (route && route->name() != new_text) {
		route->set_name (new_text);
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
		TimeAxisView* tav = _editor->axis_view_from_route (*i);
		if (tav) {
			show.insert (tav);
		}
	}

	suspend_redisplay ();

	TreeModel::Children rows = _model->children ();
	for (TreeModel::Children::iterator i = rows.begin(); i != rows.end(); ++i) {
		TimeAxisView* tv = (*i)[_columns.tv];
		(*i)[_columns.visible] = (show.find (tv) != show.end());
	}

	resume_redisplay ();
}

