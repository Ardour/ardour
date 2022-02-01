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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <list>
#include <vector>

#include "pbd/unwind.h"

#include "ardour/audio_track.h"
#include "ardour/debug.h"
#include "ardour/midi_track.h"
#include "ardour/route.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/utils.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/treeutils.h"

#include "widgets/tooltips.h"

#include "actions.h"
#include "ardour_ui.h"
#include "route_list_base.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"
#include "route_sorter.h"
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

RouteListBase::RouteListBase ()
	: _menu (0)
	, old_focus (0)
	, name_editable (0)
	, _ignore_reorder (false)
	, _ignore_visibility_change (false)
	, _ignore_selection_change (false)
	, _column_does_not_select (false)
	, _adding_routes (false)
	, _route_deletion_in_progress (false)
{
	add_name_column ();

#if 0
	setup_col (append_toggle (_columns.visible, _columns.noop_true, sigc::mem_fun (*this, &RouteListBase::on_tv_visible_changed)), S_("Visible|V"), _("Track/Bus visible ?"));
	setup_col (append_toggle (_columns.trigger, _columns.is_track, sigc::mem_fun (*this, &RouteListBase::on_tv_trigger_changed)),  S_("Cues|C"), _("Visible on Cues window ?"));
	setup_col (append_toggle (_columns.active, _columns.activatable, sigc::mem_fun (*this, &RouteListBase::on_tv_active_changed)), S_("Active|A"),  _("Track/Bus active ?"));

	append_col_input_active ();
	append_col_rec_enable ();
	append_col_rec_safe ();
	append_col_mute ();
	append_col_solo ();
#endif

	_display.set_headers_visible (true);
	_display.get_selection ()->set_mode (SELECTION_MULTIPLE);
	_display.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &RouteListBase::selection_changed));
	_display.set_reorderable (true);
	_display.set_name (X_("EditGroupList"));
	_display.set_rules_hint (true);
	_display.set_size_request (100, -1);

	_scroller.add (_display);
	_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	_model = ListStore::create (_columns);
	_display.set_model (_model);

	_display.get_selection ()->set_select_function (sigc::mem_fun (*this, &RouteListBase::select_function));

	_model->signal_row_deleted ().connect (sigc::mem_fun (*this, &RouteListBase::row_deleted));
	_model->signal_rows_reordered ().connect (sigc::mem_fun (*this, &RouteListBase::reordered));

	_display.signal_button_press_event ().connect (sigc::mem_fun (*this, &RouteListBase::button_press), false);
	_display.signal_button_release_event ().connect (sigc::mem_fun (*this, &RouteListBase::button_release), false);
	_scroller.signal_key_press_event ().connect (sigc::mem_fun (*this, &RouteListBase::key_press), false);

	_scroller.signal_focus_in_event ().connect (sigc::mem_fun (*this, &RouteListBase::focus_in), false);
	_scroller.signal_focus_out_event ().connect (sigc::mem_fun (*this, &RouteListBase::focus_out));

	_display.signal_enter_notify_event ().connect (sigc::mem_fun (*this, &RouteListBase::enter_notify), false);
	_display.signal_leave_notify_event ().connect (sigc::mem_fun (*this, &RouteListBase::leave_notify), false);

	_display.set_enable_search (false);
}

RouteListBase::~RouteListBase ()
{
	delete _menu;
}

void
RouteListBase::setup_col (Gtk::TreeViewColumn* tvc, const char* label, const char* tooltip)
{
	Gtk::Label* l = manage (new Label (label));
	set_tooltip (*l, tooltip);
	tvc->set_widget (*l);
	l->show ();
}

void
RouteListBase::add_name_column ()
{
	Gtk::TreeViewColumn* tvc = manage (new Gtk::TreeViewColumn ("", _columns.text));

	setup_col (tvc, _("Name"), ("Track/Bus name"));

	CellRendererText* cell = dynamic_cast<CellRendererText*> (tvc->get_first_cell_renderer ());
	cell->signal_editing_started ().connect (sigc::mem_fun (*this, &RouteListBase::name_edit_started));
	tvc->set_sizing (TREE_VIEW_COLUMN_FIXED);
	tvc->set_expand (true);
	tvc->set_min_width (50);
	cell->property_editable () = true;
	cell->signal_editing_started ().connect (sigc::mem_fun (*this, &RouteListBase::name_edit_started));
	cell->signal_edited ().connect (sigc::mem_fun (*this, &RouteListBase::name_edit));

	_display.append_column (*tvc);
}

void
RouteListBase::append_col_rec_enable ()
{
	CellRendererPixbufMulti* cell;
	cell = append_cell (S_("Rec|R"), _("Record enabled"), _columns.rec_state, _columns.is_track, sigc::mem_fun (*this, &RouteListBase::on_tv_rec_enable_changed));
	cell->set_pixbuf (0, ::get_icon ("record-normal-disabled"));
	cell->set_pixbuf (1, ::get_icon ("record-normal-in-progress"));
	cell->set_pixbuf (2, ::get_icon ("record-normal-enabled"));
	cell->set_pixbuf (3, ::get_icon ("record-step"));
}

void
RouteListBase::append_col_rec_safe ()
{
	CellRendererPixbufMulti* cell;
	cell = append_cell (S_("Rec|R"), _("Record enabled"), _columns.rec_safe, _columns.is_track, sigc::mem_fun (*this, &RouteListBase::on_tv_rec_safe_toggled));
	cell->set_pixbuf (0, ::get_icon ("rec-safe-disabled"));
	cell->set_pixbuf (1, ::get_icon ("rec-safe-enabled"));
}

void
RouteListBase::append_col_input_active ()
{
	CellRendererPixbufMulti* cell;
	cell = append_cell (S_("MidiInput|I"), _("MIDI input enabled"), _columns.is_input_active, _columns.is_midi, sigc::mem_fun (*this, &RouteListBase::on_tv_input_active_changed));
	cell->set_pixbuf (0, ::get_icon ("midi-input-inactive"));
	cell->set_pixbuf (1, ::get_icon ("midi-input-active"));
}

void
RouteListBase::append_col_mute ()
{
	CellRendererPixbufMulti* cell;
	cell = append_cell (S_("Mute|M"), _("Muted"), _columns.mute_state, _columns.noop_true, sigc::mem_fun (*this, &RouteListBase::on_tv_mute_enable_toggled));
	cell->set_pixbuf (Gtkmm2ext::Off, ::get_icon ("mute-disabled"));
	cell->set_pixbuf (Gtkmm2ext::ImplicitActive, ::get_icon ("muted-by-others"));
	cell->set_pixbuf (Gtkmm2ext::ExplicitActive, ::get_icon ("mute-enabled"));
}

void
RouteListBase::append_col_solo ()
{
	CellRendererPixbufMulti* cell;
	cell = append_cell (S_("Solo|S"), _("Soloed"), _columns.solo_state, _columns.solo_visible, sigc::mem_fun (*this, &RouteListBase::on_tv_solo_enable_toggled));
	cell->set_pixbuf (Gtkmm2ext::Off, ::get_icon ("solo-disabled"));
	cell->set_pixbuf (Gtkmm2ext::ExplicitActive, ::get_icon ("solo-enabled"));
	cell->set_pixbuf (Gtkmm2ext::ImplicitActive, ::get_icon ("soloed-by-others"));

	cell = append_cell (S_("SoloIso|SI"), _("Solo Isolated"), _columns.solo_isolate_state, _columns.solo_lock_iso_visible, sigc::mem_fun (*this, &RouteListBase::on_tv_solo_isolate_toggled));
	cell->set_pixbuf (0, ::get_icon ("solo-isolate-disabled"));
	cell->set_pixbuf (1, ::get_icon ("solo-isolate-enabled"));

	cell = append_cell (S_("SoloLock|SS"), _("Solo Safe (Locked)"), _columns.solo_safe_state, _columns.solo_lock_iso_visible, sigc::mem_fun (*this, &RouteListBase::on_tv_solo_safe_toggled));
	cell->set_pixbuf (0, ::get_icon ("solo-safe-disabled"));
	cell->set_pixbuf (1, ::get_icon ("solo-safe-enabled"));
}

bool
RouteListBase::focus_in (GdkEventFocus*)
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
RouteListBase::focus_out (GdkEventFocus*)
{
	if (old_focus) {
		old_focus->grab_focus ();
		old_focus = 0;
	}

	return false;
}

bool
RouteListBase::enter_notify (GdkEventCrossing*)
{
	if (name_editable) {
		return true;
	}

	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
RouteListBase::leave_notify (GdkEventCrossing*)
{
	if (old_focus) {
		old_focus->grab_focus ();
		old_focus = 0;
	}

	Keyboard::magic_widget_drop_focus ();
	return false;
}

void
RouteListBase::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	initial_display ();

	if (_session) {
		_session->vca_manager ().VCAAdded.connect (_session_connections, invalidator (_scroller), boost::bind (&RouteListBase::add_masters, this, _1), gui_context ());
		_session->RouteAdded.connect (_session_connections, invalidator (_scroller), boost::bind (&RouteListBase::add_routes, this, _1), gui_context ());
		_session->SoloChanged.connect (_session_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
		_session->RecordStateChanged.connect (_session_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
		PresentationInfo::Change.connect (_session_connections, invalidator (_scroller), boost::bind (&RouteListBase::presentation_info_changed, this, _1), gui_context ());
	}
}

void
RouteListBase::on_tv_input_active_changed (std::string const& path_string)
{
	Gtk::TreeModel::Row          row       = *_model->get_iter (Gtk::TreeModel::Path (path_string));
	boost::shared_ptr<Stripable> stripable = row[_columns.stripable];
	boost::shared_ptr<MidiTrack> mt        = boost::dynamic_pointer_cast<MidiTrack> (stripable);

	if (mt) {
		mt->set_input_active (!mt->input_active ());
	}
}

void
RouteListBase::on_tv_rec_enable_changed (std::string const& path_string)
{
	Gtk::TreeModel::Row                  row       = *_model->get_iter (Gtk::TreeModel::Path (path_string));
	boost::shared_ptr<Stripable>         stripable = row[_columns.stripable];
	boost::shared_ptr<AutomationControl> ac        = stripable->rec_enable_control ();

	if (ac) {
		ac->set_value (!ac->get_value (), Controllable::UseGroup);
	}
}

void
RouteListBase::on_tv_rec_safe_toggled (std::string const& path_string)
{
	Gtk::TreeModel::Row                  row       = *_model->get_iter (Gtk::TreeModel::Path (path_string));
	boost::shared_ptr<Stripable>         stripable = row[_columns.stripable];
	boost::shared_ptr<AutomationControl> ac (stripable->rec_safe_control ());

	if (ac) {
		ac->set_value (!ac->get_value (), Controllable::UseGroup);
	}
}

void
RouteListBase::on_tv_mute_enable_toggled (std::string const& path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row                  row       = *_model->get_iter (Gtk::TreeModel::Path (path_string));
	boost::shared_ptr<Stripable>         stripable = row[_columns.stripable];
	boost::shared_ptr<AutomationControl> ac (stripable->mute_control ());

	if (ac) {
		ac->set_value (!ac->get_value (), Controllable::UseGroup);
	}
}

void
RouteListBase::on_tv_solo_enable_toggled (std::string const& path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row                  row       = *_model->get_iter (Gtk::TreeModel::Path (path_string));
	boost::shared_ptr<Stripable>         stripable = row[_columns.stripable];
	boost::shared_ptr<AutomationControl> ac (stripable->solo_control ());

	if (ac) {
		ac->set_value (!ac->get_value (), Controllable::UseGroup);
	}
}

void
RouteListBase::on_tv_solo_isolate_toggled (std::string const& path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row                  row       = *_model->get_iter (Gtk::TreeModel::Path (path_string));
	boost::shared_ptr<Stripable>         stripable = row[_columns.stripable];
	boost::shared_ptr<AutomationControl> ac (stripable->solo_isolate_control ());

	if (ac) {
		ac->set_value (!ac->get_value (), Controllable::UseGroup);
	}
}

void
RouteListBase::on_tv_solo_safe_toggled (std::string const& path_string)
{
	// Get the model row that has been toggled.
	Gtk::TreeModel::Row                  row       = *_model->get_iter (Gtk::TreeModel::Path (path_string));
	boost::shared_ptr<Stripable>         stripable = row[_columns.stripable];
	boost::shared_ptr<AutomationControl> ac (stripable->solo_safe_control ());

	if (ac) {
		ac->set_value (!ac->get_value (), Controllable::UseGroup);
	}
}

void
RouteListBase::build_menu ()
{
	using namespace Menu_Helpers;
	using namespace Gtk;

	_menu = new Menu;

	MenuList& items = _menu->items ();
	_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Show All"), sigc::bind (sigc::mem_fun (*this, &RouteListBase::set_all_audio_midi_visibility), 0, true)));
	items.push_back (MenuElem (_("Hide All"), sigc::bind (sigc::mem_fun (*this, &RouteListBase::set_all_audio_midi_visibility), 0, false)));
	items.push_back (MenuElem (_("Show All Audio Tracks"), sigc::bind (sigc::mem_fun (*this, &RouteListBase::set_all_audio_midi_visibility), 1, true)));
	items.push_back (MenuElem (_("Hide All Audio Tracks"), sigc::bind (sigc::mem_fun (*this, &RouteListBase::set_all_audio_midi_visibility), 1, false)));
	items.push_back (MenuElem (_("Show All Midi Tracks"), sigc::bind (sigc::mem_fun (*this, &RouteListBase::set_all_audio_midi_visibility), 3, true)));
	items.push_back (MenuElem (_("Hide All Midi Tracks"), sigc::bind (sigc::mem_fun (*this, &RouteListBase::set_all_audio_midi_visibility), 3, false)));
	items.push_back (MenuElem (_("Show All Busses"), sigc::bind (sigc::mem_fun (*this, &RouteListBase::set_all_audio_midi_visibility), 2, true)));
	items.push_back (MenuElem (_("Hide All Busses"), sigc::bind (sigc::mem_fun (*this, &RouteListBase::set_all_audio_midi_visibility), 2, false)));
	items.push_back (MenuElem (_("Only Show Tracks with Regions Under Playhead"), sigc::mem_fun (*this, &RouteListBase::show_tracks_with_regions_at_playhead)));
}

void
RouteListBase::row_deleted (Gtk::TreeModel::Path const&)
{
	if (!_session || _session->deletion_in_progress ()) {
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

	if (!_route_deletion_in_progress) {
		sync_presentation_info_from_treeview ();
	}
}

void
RouteListBase::reordered (TreeModel::Path const&, TreeModel::iterator const&, int* /*what*/)
{
	/* reordering implies that RID's will change, so
	 * sync_presentation_info_from_treeview() will cause a redisplay.
	 */

	DEBUG_TRACE (DEBUG::OrderKeys, "editor routes treeview reordered\n");
	sync_presentation_info_from_treeview ();
}

void
RouteListBase::on_tv_visible_changed (std::string const& path)
{
	if (!_session || _session->deletion_in_progress ()) {
		return;
	}
	if (_ignore_visibility_change) {
		return;
	}

	DisplaySuspender ds;
	TreeIter         iter;

	if ((iter = _model->get_iter (path))) {
		bool hidden = (*iter)[_columns.visible]; // toggle -> invert flag

		boost::shared_ptr<Stripable> stripable = (*iter)[_columns.stripable];

		if (hidden != stripable->presentation_info ().hidden ()) {
			stripable->presentation_info ().set_hidden (hidden);

			boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (stripable);
			RouteGroup*              rg    = route ? route->route_group () : 0;
			if (rg && rg->is_active () && rg->is_hidden ()) {
				boost::shared_ptr<RouteList> rl (rg->route_list ());
				for (RouteList::const_iterator i = rl->begin (); i != rl->end (); ++i) {
					(*i)->presentation_info ().set_hidden (hidden);
				}
			}
		}
	}
}

void
RouteListBase::on_tv_trigger_changed (std::string const& path)
{
	if (!_session || _session->deletion_in_progress ()) {
		return;
	}

	Gtk::TreeModel::Row row = *_model->get_iter (path);
	assert (row[_columns.is_track]);
	boost::shared_ptr<Stripable> stripable = row[_columns.stripable];
	bool const                   tt        = row[_columns.trigger];
	stripable->presentation_info ().set_trigger_track (!tt);
}

void
RouteListBase::on_tv_active_changed (std::string const& path)
{
	if (!_session || _session->deletion_in_progress ()) {
		return;
	}

	Gtk::TreeModel::Row          row       = *_model->get_iter (path);
	boost::shared_ptr<Stripable> stripable = row[_columns.stripable];
	boost::shared_ptr<Route>     route     = boost::dynamic_pointer_cast<Route> (stripable);
	if (route) {
		bool const active = row[_columns.active];
		route->set_active (!active, this);
	}
}

void
RouteListBase::initial_display ()
{
	if (!_session) {
		clear ();
		return;
	}

	_model->clear ();

	StripableList sl;
	_session->get_stripables (sl);
	add_stripables (sl);

	sync_treeview_from_presentation_info (Properties::order);
}

void
RouteListBase::add_masters (VCAList& vlist)
{
	StripableList sl;

	for (VCAList::iterator v = vlist.begin (); v != vlist.end (); ++v) {
		sl.push_back (boost::dynamic_pointer_cast<Stripable> (*v));
	}

	add_stripables (sl);
}

void
RouteListBase::add_routes (RouteList& rlist)
{
	StripableList sl;

	for (RouteList::iterator r = rlist.begin (); r != rlist.end (); ++r) {
		sl.push_back (*r);
	}

	add_stripables (sl);
}

void
RouteListBase::add_stripables (StripableList& slist)
{
	PBD::Unwinder<bool> at (_adding_routes, true);

	Gtk::TreeModel::Children::iterator insert_iter = _model->children ().end ();

	slist.sort (Stripable::Sorter ());

	for (Gtk::TreeModel::Children::iterator it = _model->children ().begin (); it != _model->children ().end (); ++it) {
		boost::shared_ptr<Stripable> r = (*it)[_columns.stripable];

		if (r->presentation_info ().order () == (slist.front ()->presentation_info ().order () + slist.size ())) {
			insert_iter = it;
			break;
		}
	}

	{
		PBD::Unwinder<bool> uw (_ignore_selection_change, true);
		_display.set_model (Glib::RefPtr<ListStore> ());
	}

	for (StripableList::iterator s = slist.begin (); s != slist.end (); ++s) {
		boost::shared_ptr<Stripable> stripable = *s;
		boost::shared_ptr<MidiTrack> midi_trk;
		boost::shared_ptr<Route>     route;

		TreeModel::Row row;

		if (boost::dynamic_pointer_cast<VCA> (stripable)) {
			row = *(_model->insert (insert_iter));

			row[_columns.is_track]        = false;
			row[_columns.is_input_active] = false;
			row[_columns.is_midi]         = false;
			row[_columns.activatable]     = true;

		} else if ((route = boost::dynamic_pointer_cast<Route> (*s))) {
			if (route->is_auditioner ()) {
				continue;
			}
			if (route->is_monitor ()) {
				continue;
			}

			row = *(_model->insert (insert_iter));

			midi_trk = boost::dynamic_pointer_cast<MidiTrack> (stripable);

			row[_columns.is_track]    = (boost::dynamic_pointer_cast<Track> (stripable) != 0);
			row[_columns.activatable] = !stripable->is_master ();

			if (midi_trk) {
				row[_columns.is_input_active] = midi_trk->input_active ();
				row[_columns.is_midi]         = true;
			} else {
				row[_columns.is_input_active] = false;
				row[_columns.is_midi]         = false;
			}
		}

		row[_columns.noop_true]             = true;
		row[_columns.text]                  = stripable->name ();
		row[_columns.visible]               = !stripable->presentation_info ().hidden ();
		row[_columns.trigger]               = stripable->presentation_info ().trigger_track () && row[_columns.is_track];
		row[_columns.active]                = true;
		row[_columns.stripable]             = stripable;
		row[_columns.mute_state]            = RouteUI::mute_active_state (_session, stripable);
		row[_columns.solo_state]            = RouteUI::solo_active_state (stripable);
		row[_columns.solo_visible]          = !stripable->is_master ();
		row[_columns.solo_lock_iso_visible] = row[_columns.solo_visible] && row[_columns.activatable];
		row[_columns.solo_isolate_state]    = RouteUI::solo_isolate_active_state (stripable);
		row[_columns.solo_safe_state]       = RouteUI::solo_safe_active_state (stripable);
		row[_columns.name_editable]         = true;

		boost::weak_ptr<Stripable> ws (stripable);

		/* for now, we need both of these. PropertyChanged covers on
		 * pre-defined, "global" things of interest to a
		 * UI. gui_changed covers arbitrary, un-enumerated, un-typed
		 * changes that may only be of interest to a particular
		 * UI (e.g. track-height is not of any relevant to OSC)
		 */

		stripable->PropertyChanged.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::route_property_changed, this, _1, ws), gui_context ());
		stripable->presentation_info ().PropertyChanged.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::route_property_changed, this, _1, ws), gui_context ());

		if (boost::dynamic_pointer_cast<Track> (stripable)) {
			boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (stripable);
			t->rec_enable_control ()->Changed.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
			t->rec_safe_control ()->Changed.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
		}

		if (midi_trk) {
			midi_trk->StepEditStatusChange.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
			midi_trk->InputActiveChanged.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::update_input_active_display, this), gui_context ());
		}

		boost::shared_ptr<AutomationControl> ac;

		if ((ac = stripable->mute_control ()) != 0) {
			ac->Changed.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
		}
		if ((ac = stripable->solo_control ()) != 0) {
			ac->Changed.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
		}
		if ((ac = stripable->solo_isolate_control ()) != 0) {
			ac->Changed.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
		}
		if ((ac = stripable->solo_safe_control ()) != 0) {
			ac->Changed.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
		}

		if (route) {
			route->active_changed.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::queue_idle_update, this), gui_context ());
		}
		stripable->DropReferences.connect (_stripable_connections, invalidator (_scroller), boost::bind (&RouteListBase::remove_strip, this, ws), gui_context ());
	}

	queue_idle_update ();
	update_input_active_display ();

	{
		PBD::Unwinder<bool> uw (_ignore_selection_change, true);
		_display.set_model (_model);

		/* set the treeview model selection state */
		TreeModel::Children rows = _model->children ();
		for (TreeModel::Children::iterator ri = rows.begin (); ri != rows.end (); ++ri) {
			boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
			if (stripable && stripable->is_selected ()) {
				_display.get_selection ()->select (*ri);
			} else {
				_display.get_selection ()->unselect (*ri);
			}
		}
	}
}

void
RouteListBase::remove_strip (boost::weak_ptr<ARDOUR::Stripable> ws)
{
	boost::shared_ptr<Stripable> stripable (ws.lock ());

	if (!stripable) {
		return;
	}

	TreeModel::Children           rows = _model->children ();
	TreeModel::Children::iterator ri;

	PBD::Unwinder<bool> uw (_ignore_selection_change, true);

	for (ri = rows.begin (); ri != rows.end (); ++ri) {
		boost::shared_ptr<Stripable> s = (*ri)[_columns.stripable];
		if (s == stripable) {
			PBD::Unwinder<bool> uw (_route_deletion_in_progress, true);
			_model->erase (ri);
			break;
		}
	}
}

void
RouteListBase::route_property_changed (const PropertyChange& what_changed, boost::weak_ptr<Stripable> s)
{
	if (_adding_routes) {
		return;
	}

	PropertyChange interests;
	interests.add (ARDOUR::Properties::name);
	interests.add (ARDOUR::Properties::hidden);
	interests.add (ARDOUR::Properties::trigger_track);

	if (!what_changed.contains (interests)) {
		return;
	}

	boost::shared_ptr<Stripable> stripable = s.lock ();

	if (!stripable) {
		return;
	}

	TreeModel::Children rows = _model->children ();
	for (TreeModel::Children::iterator i = rows.begin (); i != rows.end (); ++i) {
		boost::shared_ptr<Stripable> ss = (*i)[_columns.stripable];

		if (ss != stripable) {
			continue;
		}

		if (what_changed.contains (ARDOUR::Properties::name)) {
			(*i)[_columns.text] = stripable->name ();
		}

		if (what_changed.contains (ARDOUR::Properties::hidden)) {
			(*i)[_columns.visible] = !stripable->presentation_info ().hidden ();
		}

		if (what_changed.contains (ARDOUR::Properties::trigger_track)) {
			(*i)[_columns.trigger] = stripable->presentation_info ().trigger_track () && (*i)[_columns.is_track];
		}

		break;
	}
}

void
RouteListBase::presentation_info_changed (PropertyChange const& what_changed)
{
	PropertyChange soh;

	soh.add (Properties::order);
	soh.add (Properties::selected);
	if (what_changed.contains (soh)) {
		sync_treeview_from_presentation_info (what_changed);
	}
}

void
RouteListBase::sync_presentation_info_from_treeview ()
{
	if (_ignore_reorder || !_session || _session->deletion_in_progress ()) {
		return;
	}

	TreeModel::Children rows = _model->children ();

	if (rows.empty ()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "editor sync presentation info from treeview\n");

	PresentationInfo::order_t order = 0;

	PresentationInfo::ChangeSuspender cs;

	for (TreeModel::Children::iterator ri = rows.begin (); ri != rows.end (); ++ri) {
		boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
		bool                         visible   = (*ri)[_columns.visible];

		stripable->presentation_info ().set_hidden (!visible);
		stripable->set_presentation_order (order);
		++order;
	}
}

void
RouteListBase::sync_treeview_from_presentation_info (PropertyChange const& what_changed)
{
	/* Some route order key(s) have been changed, make sure that
	 * we update out tree/list model and GUI to reflect the change.
	 */

	if (_ignore_reorder || !_session || _session->deletion_in_progress ()) {
		return;
	}

	TreeModel::Children rows = _model->children ();
	if (rows.empty ()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "editor sync model from presentation info.\n");

	bool changed = false;

	if (what_changed.contains (Properties::order)) {
		vector<int> neworder;
		uint32_t    old_order = 0;

		TreeOrderKeys sorted;
		for (TreeModel::Children::iterator ri = rows.begin (); ri != rows.end (); ++ri, ++old_order) {
			boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
			/* use global order */
			sorted.push_back (TreeOrderKey (old_order, stripable));
		}

		TreeOrderKeySorter cmp;

		sort (sorted.begin (), sorted.end (), cmp);
		neworder.assign (sorted.size (), 0);

		uint32_t n = 0;

		for (TreeOrderKeys::iterator sr = sorted.begin (); sr != sorted.end (); ++sr, ++n) {
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

			_display.unset_model ();
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
		for (TreeModel::Children::iterator ri = rows.begin (); ri != rows.end (); ++ri) {
			boost::shared_ptr<Stripable> stripable = (*ri)[_columns.stripable];
			if (stripable && stripable->is_selected ()) {
				_display.get_selection ()->select (*ri);
			} else {
				_display.get_selection ()->unselect (*ri);
			}
		}
	}
}

void
RouteListBase::set_all_audio_midi_visibility (int which, bool yn)
{
	TreeModel::Children           rows = _model->children ();
	TreeModel::Children::iterator i;

	DisplaySuspender    ds;
	PBD::Unwinder<bool> uw (_ignore_visibility_change, true);

	for (i = rows.begin (); i != rows.end (); ++i) {
		boost::shared_ptr<Stripable> stripable = (*i)[_columns.stripable];

		/*
		 * which = 0: any (incl. VCA)
		 * which = 1: audio-tracks
		 * which = 2: busses
		 * which = 3: midi-tracks
		 */
		bool is_audio = boost::dynamic_pointer_cast<AudioTrack> (stripable) != 0;
		bool is_midi  = boost::dynamic_pointer_cast<MidiTrack> (stripable) != 0;
		bool is_bus   = !is_audio && !is_midi && boost::dynamic_pointer_cast<Route> (stripable) != 0;

		switch (which) {
			case 0:
				(*i)[_columns.visible] = yn;
				break;
			case 1:
				if (is_audio) {
					(*i)[_columns.visible] = yn;
				}
				break;
			case 2:
				if (is_bus) {
					(*i)[_columns.visible] = yn;
				}
				break;
			case 3:
				if (is_midi) {
					(*i)[_columns.visible] = yn;
				}
				break;
			default:
				assert (0);
		}
	}

	sync_presentation_info_from_treeview ();
}

bool
RouteListBase::key_press (GdkEventKey* ev)
{
	TreeViewColumn* col;
	TreePath        path;

	boost::shared_ptr<RouteList> rl (new RouteList);

	switch (ev->keyval) {
		case GDK_Tab:
		case GDK_ISO_Left_Tab:

			/* If we appear to be editing something, leave that cleanly and appropriately. */
			if (name_editable) {
				name_editable->editing_done ();
				name_editable = 0;
			}

			col = _display.get_column (0); /* track-name col */

			if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
				treeview_select_previous (_display, _model, col);
			} else {
				treeview_select_next (_display, _model, col);
			}

			return true;
			break;

		case 'm':
			if (get_relevant_routes (rl)) {
				_session->set_controls (route_list_to_control_list (rl, &Stripable::mute_control), rl->front ()->muted () ? 0.0 : 1.0, Controllable::NoGroup);
			}
			return true;
			break;

		case 's':
			if (get_relevant_routes (rl)) {
				_session->set_controls (route_list_to_control_list (rl, &Stripable::solo_control), rl->front ()->self_soloed () ? 0.0 : 1.0, Controllable::NoGroup);
			}
			return true;
			break;

		case 'r':
			if (get_relevant_routes (rl)) {
				for (RouteList::const_iterator r = rl->begin (); r != rl->end (); ++r) {
					boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (*r);
					if (t) {
						_session->set_controls (route_list_to_control_list (rl, &Stripable::rec_enable_control), !t->rec_enable_control ()->get_value (), Controllable::NoGroup);
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
RouteListBase::get_relevant_routes (boost::shared_ptr<RouteList> rl)
{
	RefPtr<TreeSelection> selection = _display.get_selection ();
	TreePath              path;
	TreeIter              iter;

	if (selection->count_selected_rows () != 0) {
		/* use selection */

		RefPtr<TreeModel> tm = RefPtr<TreeModel>::cast_dynamic (_model);
		iter                 = selection->get_selected (tm);

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
		boost::shared_ptr<Stripable> stripable = (*iter)[_columns.stripable];
		boost::shared_ptr<Route>     route     = boost::dynamic_pointer_cast<Route> (stripable);
		if (route) {
			rl->push_back (route);
		}
	}

	return !rl->empty ();
}

bool
RouteListBase::select_function (const Glib::RefPtr<Gtk::TreeModel>&, const Gtk::TreeModel::Path&, bool)
{
	return !_column_does_not_select;
}

bool
RouteListBase::button_release (GdkEventButton*)
{
	_column_does_not_select = false;
	return false;
}

bool
RouteListBase::button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		if (_menu == 0) {
			build_menu ();
		}
		_menu->popup (ev->button, ev->time);
		return true;
	}

	TreeModel::Path path;
	TreeViewColumn* tvc;
	int             cell_x;
	int             cell_y;

	if (!_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, tvc, cell_x, cell_y)) {
		/* cancel selection */
		_display.get_selection ()->unselect_all ();
		/* end any editing by grabbing focus */
		_display.grab_focus ();
		return true;
	}

	if (no_select_columns.find (tvc) != no_select_columns.end ()) {
		_column_does_not_select = true;
	}

	//Scroll editor canvas to selected track
	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		PublicEditor&       e (PublicEditor::instance ());
		Gtk::TreeModel::Row row = *_model->get_iter (path);
		TimeAxisView*       tv  = e.time_axis_view_from_stripable (row[_columns.stripable]);
		if (tv) {
			e.ensure_time_axis_view_is_visible (*tv, true);
		}
	}

	return false;
}

void
RouteListBase::selection_changed ()
{
	if (_ignore_selection_change || _column_does_not_select) {
		return;
	}

	PublicEditor& e (PublicEditor::instance ());
	TrackViewList selected;

	if (_display.get_selection ()->count_selected_rows () > 0) {
		TreeView::Selection::ListHandle_Path rows = _display.get_selection ()->get_selected_rows ();

		for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin (); i != rows.end (); ++i) {
			TreeIter iter;
			if ((iter = _model->get_iter (*i))) {
				TimeAxisView* tv = e.time_axis_view_from_stripable ((*iter)[_columns.stripable]);
				if (tv) {
					selected.push_back (tv);
				}
			}
		}
	}

	e.begin_reversible_selection_op (X_("Select Track from Route List"));
	Selection& s (e.get_selection ());

	if (selected.empty ()) {
		s.clear_tracks ();
	} else {
		s.set (selected);
		e.ensure_time_axis_view_is_visible (*(selected.front ()), true);
	}

	e.commit_reversible_selection_op ();
}

void
RouteListBase::update_input_active_display ()
{
	TreeModel::Children           rows = _model->children ();
	TreeModel::Children::iterator i;

	for (i = rows.begin (); i != rows.end (); ++i) {
		boost::shared_ptr<Stripable> stripable = (*i)[_columns.stripable];

		if (boost::dynamic_pointer_cast<Track> (stripable)) {
			boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (stripable);

			if (mt) {
				(*i)[_columns.is_input_active] = mt->input_active ();
			}
		}
	}
}

void
RouteListBase::queue_idle_update ()
{
	if (!_idle_update_connection.connected ()) {
		_idle_update_connection = Glib::signal_idle ().connect (sigc::mem_fun (*this, &RouteListBase::idle_update_mute_rec_solo_etc));
	}
}

bool
RouteListBase::idle_update_mute_rec_solo_etc ()
{
	TreeModel::Children           rows = _model->children ();
	TreeModel::Children::iterator i;

	for (i = rows.begin (); i != rows.end (); ++i) {
		boost::shared_ptr<Stripable> stripable = (*i)[_columns.stripable];
		boost::shared_ptr<Route>     route     = boost::dynamic_pointer_cast<Route> (stripable);
		(*i)[_columns.mute_state]              = RouteUI::mute_active_state (_session, stripable);
		(*i)[_columns.solo_state]              = RouteUI::solo_active_state (stripable);
		(*i)[_columns.solo_isolate_state]      = RouteUI::solo_isolate_active_state (stripable) ? 1 : 0;
		(*i)[_columns.solo_safe_state]         = RouteUI::solo_safe_active_state (stripable) ? 1 : 0;
		if (route) {
			(*i)[_columns.active] = route->active ();
		} else {
			(*i)[_columns.active] = true;
		}

		boost::shared_ptr<Track> trk (boost::dynamic_pointer_cast<Track> (route));

		if (trk) {
			boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (route);

			if (trk->rec_enable_control ()->get_value ()) {
				if (_session->record_status () == Session::Recording) {
					(*i)[_columns.rec_state] = 1;
				} else {
					(*i)[_columns.rec_state] = 2;
				}
			} else if (mt && mt->step_editing ()) {
				(*i)[_columns.rec_state] = 3;
			} else {
				(*i)[_columns.rec_state] = 0;
			}

			(*i)[_columns.rec_safe]      = trk->rec_safe_control ()->get_value ();
			(*i)[_columns.name_editable] = !trk->rec_enable_control ()->get_value ();
		}
	}

	return false; // do not call again (until needed)
}

void
RouteListBase::clear ()
{
	PBD::Unwinder<bool> uw (_ignore_selection_change, true);
	_stripable_connections.drop_connections ();
	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->clear ();
	_display.set_model (_model);
}

void
RouteListBase::name_edit_started (CellEditable* ce, const Glib::ustring&)
{
	name_editable = ce;

	/* give it a special name */

	Gtk::Entry* e = dynamic_cast<Gtk::Entry*> (ce);

	if (e) {
		e->set_name (X_("RouteNameEditorEntry"));
	}
}

void
RouteListBase::name_edit (std::string const& path, std::string const& new_text)
{
	name_editable = 0;

	TreeIter iter = _model->get_iter (path);

	if (!iter) {
		return;
	}

	boost::shared_ptr<Stripable> stripable = (*iter)[_columns.stripable];

	if (stripable && stripable->name () != new_text) {
		stripable->set_name (new_text);
	}
}

void
RouteListBase::show_tracks_with_regions_at_playhead ()
{
	boost::shared_ptr<RouteList> const r = _session->get_routes_with_regions_at (timepos_t (_session->transport_sample ()));

	DisplaySuspender ds;

	TreeModel::Children rows = _model->children ();
	for (TreeModel::Children::iterator i = rows.begin (); i != rows.end (); ++i) {
		boost::shared_ptr<Stripable> stripable = (*i)[_columns.stripable];
		boost::shared_ptr<Route>     route     = boost::dynamic_pointer_cast<Route> (stripable);

		bool to_show = std::find (r->begin (), r->end (), route) != r->end ();
		stripable->presentation_info ().set_hidden (!to_show);
	}
}
