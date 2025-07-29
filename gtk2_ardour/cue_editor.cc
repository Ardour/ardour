/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ytkmm/scrollbar.h"

#include "widgets/ardour_icon.h"
#include "widgets/tooltips.h"

#include "pbd/controllable.h"

#include "ardour/midi_region.h"
#include "ardour/smf_source.h"
#include "ardour/types.h"

#include "canvas/canvas.h"

#include "gtkmm2ext/bindings.h"

#include "ardour_ui.h"
#include "cue_editor.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "timers.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Temporal;

CueEditor::CueEditor (std::string const & name, bool with_transport)
	: EditingContext (name)
	, HistoryOwner (name)
	, _canvas_viewport (horizontal_adjustment, vertical_adjustment)
	, _canvas (*_canvas_viewport.canvas ())
	, with_transport_controls (with_transport)
	, length_label (X_("Record:"))
	, solo_button (S_("Solo|S"))
	, zoom_in_allocate (false)
	, timebar_height (15.)
	, n_timebars (0)
{
	_canvas_hscrollbar = manage (new Gtk::HScrollbar (horizontal_adjustment));
	_canvas_hscrollbar->show ();
	horizontal_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &CueEditor::scrolled));

	_history.Changed.connect (history_connection, invalidator (*this), std::bind (&CueEditor::history_changed, this), gui_context());
	set_zoom_focus (Editing::ZoomFocusLeft);
}

CueEditor::~CueEditor ()
{
	delete own_bindings;
}

void
CueEditor::set_snapped_cursor_position (Temporal::timepos_t const & pos)
{
}

std::vector<MidiRegionView*>
CueEditor::filter_to_unique_midi_region_views (RegionSelection const & ms) const
{
	std::vector<MidiRegionView*> mrv;
	return mrv;
}

void
CueEditor::get_regionviews_by_id (PBD::ID const id, RegionSelection & regions) const
{
}

StripableTimeAxisView*
CueEditor::get_stripable_time_axis_by_id (const PBD::ID& id) const
{
	return nullptr;
}

TrackViewList
CueEditor::axis_views_from_routes (std::shared_ptr<ARDOUR::RouteList>) const
{
	TrackViewList tvl;
	return tvl;
}

ARDOUR::Location*
CueEditor::find_location_from_marker (ArdourMarker*, bool&) const
{
	return nullptr;
}

ArdourMarker*
CueEditor::find_marker_from_location_id (PBD::ID const&, bool) const
{
	return nullptr;
}

TempoMarker*
CueEditor::find_marker_for_tempo (Temporal::TempoPoint const &)
{
	return nullptr;
}

MeterMarker*
CueEditor::find_marker_for_meter (Temporal::MeterPoint const &)
{
	return nullptr;
}

void
CueEditor::redisplay_grid (bool immediate_redraw)
{
	update_grid ();
}

Temporal::timecnt_t
CueEditor::get_nudge_distance (Temporal::timepos_t const & pos, Temporal::timecnt_t& next) const
{
	return Temporal::timecnt_t (Temporal::AudioTime);
}

void
CueEditor::instant_save()
{
	if (!_region) {
		return;
	}

	/* derived classes should set other fields first, then call parent */

	region_ui_settings.follow_playhead = follow_playhead();
	region_ui_settings.samples_per_pixel = samples_per_pixel;
	region_ui_settings.grid_type = grid_type ();

	std::pair<RegionUISettingsManager::iterator,bool> res (ARDOUR_UI::instance()->region_ui_settings_manager.insert (std::make_pair (_region->id(), region_ui_settings)));

	if (!res.second) {
		/* region (ID) already present, set contents */
		res.first->second = region_ui_settings;
	}
}

void
CueEditor::begin_selection_op_history ()
{
}

void
CueEditor::begin_reversible_selection_op (std::string cmd_name)
{
}

void
CueEditor::commit_reversible_selection_op ()
{
}

void
CueEditor::abort_reversible_selection_op ()
{
}

void
CueEditor::undo_selection_op ()
{
}

void
CueEditor::redo_selection_op ()
{
}

double
CueEditor::get_y_origin () const
{
	return 0.;
}

void
CueEditor::set_zoom_focus (Editing::ZoomFocus zf)
{
	using namespace Editing;

	/* We don't allow playhead for zoom focus here */

	if (zf == ZoomFocusPlayhead) {
		return;
	}

	std::string str = zoom_focus_strings[(int)zf];

	if (str != zoom_focus_selector.get_text()) {
		zoom_focus_selector.set_text (str);
	}

	if (_zoom_focus != zf) {
		_zoom_focus = zf;
		ZoomFocusChanged (); /* EMIT SIGNAL */
	}
}

void
CueEditor::set_samples_per_pixel (samplecnt_t n)
{
	samples_per_pixel = n;
	ZoomChanged(); /* EMIT SIGNAL */
}

samplecnt_t
CueEditor::get_current_zoom () const
{
	return samples_per_pixel;
}

void
CueEditor::reposition_and_zoom (samplepos_t pos, double spp)
{
	pending_visual_change.add (VisualChange::ZoomLevel);
	pending_visual_change.samples_per_pixel = spp;

	pending_visual_change.add (VisualChange::TimeOrigin);
	pending_visual_change.time_origin = pos;

	ensure_visual_change_idle_handler ();
}

void
CueEditor::set_mouse_mode (Editing::MouseMode, bool force)
{
}

void
CueEditor::step_mouse_mode (bool next)
{
}

Gdk::Cursor*
CueEditor::get_canvas_cursor () const
{
	return nullptr;
}

std::shared_ptr<Temporal::TempoMap const>
CueEditor::start_local_tempo_map (std::shared_ptr<Temporal::TempoMap> map)
{
	std::shared_ptr<Temporal::TempoMap const> tmp = Temporal::TempoMap::use();
	Temporal::TempoMap::set (map);
	return tmp;
}

void
CueEditor::end_local_tempo_map (std::shared_ptr<Temporal::TempoMap const> map)
{
	Temporal::TempoMap::set (map);
}

void
CueEditor::do_undo (uint32_t n)
{
	if (_drags->active ()) {
		_drags->abort ();
	}

	_history.undo (n);
}

void
CueEditor::do_redo (uint32_t n)
{
	if (_drags->active ()) {
		_drags->abort ();
	}

	_history.redo (n);
}

void
CueEditor::history_changed ()
{
	update_undo_redo_actions (_history);
}

Temporal::timepos_t
CueEditor::_get_preferred_edit_position (Editing::EditIgnoreOption ignore, bool from_context_menu, bool from_outside_canvas)
{
	samplepos_t where;
	bool in_track_canvas = false;

	if (!mouse_sample (where, in_track_canvas)) {
		return Temporal::timepos_t (0);
	}

	return Temporal::timepos_t (where);
}

void
CueEditor::build_upper_toolbar ()
{
	using namespace Gtk::Menu_Helpers;

	Gtk::HBox* mode_box = manage(new Gtk::HBox);
	mode_box->set_border_width (2);
	mode_box->set_spacing(2);

	Gtk::HBox* mouse_mode_box = manage (new Gtk::HBox);
	Gtk::HBox* mouse_mode_hbox = manage (new Gtk::HBox);
	Gtk::VBox* mouse_mode_vbox = manage (new Gtk::VBox);
	Gtk::Alignment* mouse_mode_align = manage (new Gtk::Alignment);

	Glib::RefPtr<Gtk::SizeGroup> mouse_mode_size_group = Gtk::SizeGroup::create (Gtk::SIZE_GROUP_VERTICAL);
	mouse_mode_size_group->add_widget (mouse_draw_button);
	mouse_mode_size_group->add_widget (mouse_content_button);

	mouse_mode_size_group->add_widget (grid_type_selector);
	mouse_mode_size_group->add_widget (draw_length_selector);
	mouse_mode_size_group->add_widget (draw_velocity_selector);
	mouse_mode_size_group->add_widget (draw_channel_selector);
	mouse_mode_size_group->add_widget (snap_mode_button);

	mouse_mode_hbox->set_spacing (2);
	mouse_mode_hbox->pack_start (mouse_draw_button, false, false);
	mouse_mode_hbox->pack_start (mouse_content_button, false, false);

	mouse_mode_vbox->pack_start (*mouse_mode_hbox);

	mouse_mode_align->add (*mouse_mode_vbox);
	mouse_mode_align->set (0.5, 1.0, 0.0, 0.0);

	mouse_mode_box->pack_start (*mouse_mode_align, false, false);

	pack_snap_box ();
	pack_draw_box (false);

	Gtk::HBox* _toolbar_inner = manage (new Gtk::HBox);
	Gtk::HBox* _toolbar_outer = manage (new Gtk::HBox);
	Gtk::HBox* _toolbar_left = manage (new Gtk::HBox);

	_toolbar_inner->pack_start (*mouse_mode_box, false, false);
	pack_inner (*_toolbar_inner);

	set_tooltip (full_zoom_button, _("Zoom to full clip"));
	set_tooltip (note_mode_button, _("Toggle between drum and regular note drawing"));

	play_button.set_icon (ArdourIcon::TransportPlay);
	play_button.set_name ("transport button");
	play_button.show();

	if (with_transport_controls) {
		loop_button.set_icon (ArdourIcon::TransportLoop);
		loop_button.set_name ("transport button");

		solo_button.set_name ("solo button");

		play_box.set_spacing (8);
		play_box.pack_start (play_button, false, false);
		play_box.pack_start (loop_button, false, false);
		play_box.pack_start (solo_button, false, false);
		loop_button.show();
		solo_button.show();
		play_box.set_no_show_all (true);
		play_box.show ();

		play_button.signal_button_release_event().connect (sigc::mem_fun (*this, &CueEditor::play_button_press), false);
		solo_button.signal_button_release_event().connect (sigc::mem_fun (*this, &CueEditor::solo_button_press), false);
		loop_button.signal_button_release_event().connect (sigc::mem_fun (*this, &CueEditor::loop_button_press), false);
	} else {
		rec_box.pack_start (play_button, false, false);
		play_button.signal_button_release_event().connect (sigc::mem_fun (*this, &CueEditor::bang_button_press), false);
	}

	rec_enable_button.set_icon (ArdourIcon::RecButton);
	rec_enable_button.set_sensitive (false);
	rec_enable_button.signal_button_release_event().connect (sigc::mem_fun (*this, &CueEditor::rec_button_press), false);
	rec_enable_button.set_name ("record enable button");

	length_selector.add_menu_elem (MenuElem (_("Until Stopped"), sigc::bind (sigc::mem_fun (*this, &CueEditor::set_recording_length), Temporal::BBT_Offset ())));
	length_selector.add_menu_elem (MenuElem (_("1 Bar"), sigc::bind (sigc::mem_fun (*this, &CueEditor::set_recording_length), Temporal::BBT_Offset (1, 0, 0))));
	std::vector<int> b ({ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 20, 24, 32 });
	for (auto & n : b) {
		length_selector.add_menu_elem (MenuElem (string_compose (_("%1 Bars"), n), sigc::bind (sigc::mem_fun (*this, &CueEditor::set_recording_length), Temporal::BBT_Offset (n, 0, 0))));
	}
	length_selector.set_active (_("Until Stopped"));

	rec_box.set_spacing (12);
	rec_box.pack_start (rec_enable_button, false, false);
	rec_box.pack_start (length_label, false, false);
	rec_box.pack_start (length_selector, false, false);
	rec_enable_button.show();
	length_label.show ();
	length_selector.show ();
	rec_box.set_no_show_all (true);
	/* rec box not shown */

	_toolbar_outer->set_border_width (6);
	_toolbar_outer->set_spacing (12);

	pack_outer (*_toolbar_outer);

	_toolbar_outer->pack_start (*_toolbar_inner, true, false);

	build_zoom_focus_menu ();
	zoom_focus_selector.set_text (zoom_focus_strings[(int)_zoom_focus]);

	_toolbar_left->pack_start (zoom_in_button, false, false);
	_toolbar_left->pack_start (zoom_out_button, false, false);
	_toolbar_left->pack_start (full_zoom_button, false, false);
	_toolbar_left->pack_start (zoom_focus_selector, false, false);

	_toolbar_outer->pack_start (*_toolbar_left, true, false);
	_toolbox.pack_start (*_toolbar_outer, false, false);

	_contents.add (_toolbox);
	_contents.signal_unmap().connect ([this]()  { get_canvas_viewport()->unmap (); }, false);
	_contents.signal_map().connect ([this]() { get_canvas_viewport()->map (); }, false);
}

void
CueEditor::build_zoom_focus_menu ()
{
	using namespace Gtk::Menu_Helpers;
	using namespace Editing;

	zoom_focus_selector.add_menu_elem (MenuElem (zoom_focus_strings[(int)ZoomFocusLeft], sigc::bind (sigc::mem_fun(*this, &EditingContext::zoom_focus_selection_done), (ZoomFocus) ZoomFocusLeft)));
	zoom_focus_selector.add_menu_elem (MenuElem (zoom_focus_strings[(int)ZoomFocusRight], sigc::bind (sigc::mem_fun(*this, &EditingContext::zoom_focus_selection_done), (ZoomFocus) ZoomFocusRight)));
	zoom_focus_selector.add_menu_elem (MenuElem (zoom_focus_strings[(int)ZoomFocusCenter], sigc::bind (sigc::mem_fun(*this, &EditingContext::zoom_focus_selection_done), (ZoomFocus) ZoomFocusCenter)));
	zoom_focus_selector.add_menu_elem (MenuElem (zoom_focus_strings[(int)ZoomFocusMouse], sigc::bind (sigc::mem_fun(*this, &EditingContext::zoom_focus_selection_done), (ZoomFocus) ZoomFocusMouse)));
	zoom_focus_selector.set_sizing_texts (zoom_focus_strings);
}

bool
CueEditor::bang_button_press (GdkEventButton* ev)
{
	if (!ref.trigger()) {
		return true;
	}

	ref.trigger()->bang ();

	return true;
}

bool
CueEditor::play_button_press (GdkEventButton* ev)
{
	if (_session && _region) {
		_session->request_locate (_region->position().samples());
		_session->request_roll ();
	}

	return true;
}

bool
CueEditor::loop_button_press (GdkEventButton* ev)
{
	if (!_region) {
		return true;
	}

	if (_session->get_play_loop()) {
		_session->request_play_loop (false);
	} else {
		set_loop_range (_region->position(), _region->end(), _("loop region"));
		_session->request_play_loop (true);
	}

	return true;
}

bool
CueEditor::solo_button_press (GdkEventButton* ev)
{
	if (!_track) {
		return true;
	}

	_track->solo_control()->set_value (!_track->solo_control()->get_value(), PBD::Controllable::NoGroup);

	return true;
}

bool
CueEditor::rec_button_press (GdkEventButton* ev)
{
	if (ev->button != 1) {
		return false;
	}

	TriggerPtr trigger (ref.trigger());

	if (!trigger) {
		return true;
	}

	if (trigger->armed()) {
		trigger->disarm ();
	} else {
		trigger->arm (rec_length);
	}

	return true;
}

void
CueEditor::blink_rec_enable (bool onoff)
{
	if (onoff) {
		rec_enable_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		rec_enable_button.set_active_state (Gtkmm2ext::Off);
	}
}

void
CueEditor::trigger_arm_change ()
{
	if (!ref.trigger()) {
		return;
	}

	if (!ref.trigger()->armed()) {
		end_write ();
	} else {
		maybe_set_count_in ();
	}

	rec_enable_change ();
}

void
CueEditor::rec_enable_change ()
{
	if (!ref.box()) {
		return;
	}

	rec_blink_connection.disconnect ();
	count_in_connection.disconnect ();

	std::cerr << "REC, state " << ref.box()->record_enabled() << std::endl;

	switch (ref.box()->record_enabled()) {
	case Recording:
		rec_enable_button.set_active_state (Gtkmm2ext::ExplicitActive);
		rec_blink_connection.disconnect ();
		begin_write ();
		break;
	case Enabled:
		if (!UIConfiguration::instance().get_no_strobe() && ref.trigger()->armed()) {
			rec_blink_connection = Timers::blink_connect (sigc::mem_fun (*this, &CueEditor::blink_rec_enable));
		} else {
			rec_enable_button.set_active_state (Gtkmm2ext::Off);
		}
		maybe_set_count_in ();
		break;
	case Disabled:
		rec_enable_button.set_active_state (Gtkmm2ext::Off);
		break;
	}
}

void
CueEditor::set_recording_length (Temporal::BBT_Offset dur)
{
	rec_length = dur;
}

void
CueEditor::scrolled ()
{
	pending_visual_change.add (VisualChange::TimeOrigin);
	pending_visual_change.time_origin = horizontal_adjustment.get_value() * samples_per_pixel;
	ensure_visual_change_idle_handler ();
}

bool
CueEditor::canvas_pre_event (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		if (canvas_enter_leave (&ev->crossing)) {
			return true;
		}
		break;
	default:
		break;
	}

	return false;
}

bool
CueEditor::autoscroll_active () const
{
	return autoscroll_connection.connected ();
}

/** @param allow_horiz true to allow horizontal autoscroll, otherwise false.
 *
 *  @param allow_vert true to allow vertical autoscroll, otherwise false.
 *
 */
void
CueEditor::maybe_autoscroll (bool allow_horiz, bool allow_vert, bool from_headers)
{
	if (!UIConfiguration::instance().get_autoscroll_editor () || autoscroll_active ()) {
		return;
	}

	/* define a rectangular boundary for scrolling. If the mouse moves
	 * outside of this area and/or continue to be outside of this area,
	 * then we will continuously auto-scroll the canvas in the appropriate
	 * direction(s)
	 *
	 * the boundary is defined in coordinates relative to canvas' own
	 * window since that is what we're going to call ::get_pointer() on
	 * during autoscrolling to determine if we're still outside the
	 * boundary or not.
	 */

	ArdourCanvas::Rect scrolling_boundary;
	Gtk::Allocation alloc;

	alloc = get_canvas()->get_allocation ();

	alloc.set_x (0);
	alloc.set_y (0);

	if (allow_vert) {
		/* reduce height by the height of the timebars, which happens
		   to correspond to the position of the data_group.
		*/

		alloc.set_height (alloc.get_height() - data_group->position().y);
		alloc.set_y (alloc.get_y() + data_group->position().y);

		/* now reduce it again so that we start autoscrolling before we
		 * move off the top or bottom of the canvas
		 */

		alloc.set_height (alloc.get_height() - 20);
		alloc.set_y (alloc.get_y() + 10);
	}

	if (allow_horiz && (alloc.get_width() > 20)) {

		manage_possible_header (alloc);

		/* the effective width of the autoscroll boundary so
		   that we start scrolling before we hit the edge.

		   this helps when the window is slammed up against the
		   right edge of the screen, making it hard to scroll
		   effectively.
		*/

		alloc.set_width (alloc.get_width() - 20);
		alloc.set_x (alloc.get_x() + 10);
	}

	scrolling_boundary = ArdourCanvas::Rect (alloc.get_x(), alloc.get_y(), alloc.get_x() + alloc.get_width(), alloc.get_y() + alloc.get_height());

	int x, y;
	Gdk::ModifierType mask;

	get_canvas()->get_window()->get_pointer (x, y, mask);

	if ((allow_horiz && ((x < scrolling_boundary.x0 && _leftmost_sample > 0) || x >= scrolling_boundary.x1)) ||
	    (allow_vert && ((y < scrolling_boundary.y0 && vertical_adjustment.get_value() > 0)|| y >= scrolling_boundary.y1))) {
		start_canvas_autoscroll (allow_horiz, allow_vert, scrolling_boundary);
	}
}

bool
CueEditor::autoscroll_canvas ()
{
	using std::max;
	using std::min;
	int x, y;
	Gdk::ModifierType mask;
	sampleoffset_t dx = 0;
	bool no_stop = false;
	Gtk::Window* toplevel = dynamic_cast<Gtk::Window*> (_canvas_viewport.get_toplevel());

	if (!toplevel) {
		return false;
	}

	get_canvas()->get_window()->get_pointer (x, y, mask);

	VisualChange vc;
	bool vertical_motion = false;

	if (autoscroll_horizontal_allowed) {

		samplepos_t new_sample = _leftmost_sample;

		/* horizontal */

		if (x > autoscroll_boundary.x1) {

			/* bring it back into view */
			dx = x - autoscroll_boundary.x1;
			dx += 10 + (2 * (autoscroll_cnt/2));

			dx = pixel_to_sample (dx);

			dx *= UIConfiguration::instance().get_draggable_playhead_speed();

			if (_leftmost_sample < max_samplepos - dx) {
				new_sample = _leftmost_sample + dx;
			} else {
				new_sample = max_samplepos;
			}

			no_stop = true;

		} else if (x < autoscroll_boundary.x0) {

			dx = autoscroll_boundary.x0 - x;
			dx += 10 + (2 * (autoscroll_cnt/2));

			dx = pixel_to_sample (dx);

			dx *= UIConfiguration::instance().get_draggable_playhead_speed();

			if (_leftmost_sample >= dx) {
				new_sample = _leftmost_sample - dx;
			} else {
				new_sample = 0;
			}

			no_stop = true;
		}

		if (new_sample != _leftmost_sample) {
			vc.time_origin = new_sample;
			vc.add (VisualChange::TimeOrigin);
		}
	}

	if (autoscroll_vertical_allowed) {

		// const double vertical_pos = vertical_adjustment.get_value();
		const int speed_factor = 10;

		/* vertical */

		if (y < autoscroll_boundary.y0) {

			/* scroll to make higher tracks visible */

			if (autoscroll_cnt && (autoscroll_cnt % speed_factor == 0)) {
				// XXX SCROLL UP
				vertical_motion = true;
			}
			no_stop = true;

		} else if (y > autoscroll_boundary.y1) {

			if (autoscroll_cnt && (autoscroll_cnt % speed_factor == 0)) {
				// XXX SCROLL DOWN
				vertical_motion = true;
			}
			no_stop = true;
		}

	}

	if (vc.pending || vertical_motion) {

		/* change horizontal first */

		if (vc.pending) {
			visual_changer (vc);
		}

		/* now send a motion event to notify anyone who cares
		   that we have moved to a new location (because we scrolled)
		*/

		GdkEventMotion ev;

		ev.type = GDK_MOTION_NOTIFY;
		ev.state = Gdk::BUTTON1_MASK;

		/* the motion handler expects events in canvas coordinate space */

		/* we asked for the mouse position above (::get_pointer()) via
		 * our own top level window (we being the Editor). Convert into
		 * coordinates within the canvas window.
		 */

		int cx;
		int cy;

		//toplevel->translate_coordinates (*get_canvas(), x, y, cx,
		//cy);
		cx = x;
		cy = y;

		/* clamp x and y to remain within the autoscroll boundary,
		 * which is defined in window coordinates
		 */

		x = min (max ((ArdourCanvas::Coord) cx, autoscroll_boundary.x0), autoscroll_boundary.x1);
		y = min (max ((ArdourCanvas::Coord) cy, autoscroll_boundary.y0), autoscroll_boundary.y1);

		/* now convert from Editor window coordinates to canvas
		 * window coordinates
		 */

		ArdourCanvas::Duple d = get_canvas()->window_to_canvas (ArdourCanvas::Duple (cx, cy));
		ev.x = d.x;
		ev.y = d.y;
		ev.state = mask;

		motion_handler (0, (GdkEvent*) &ev, true);

	} else if (no_stop) {

		/* not changing visual state but pointer is outside the scrolling boundary
		 * so we still need to deliver a fake motion event
		 */

		GdkEventMotion ev;

		ev.type = GDK_MOTION_NOTIFY;
		ev.state = Gdk::BUTTON1_MASK;

		/* the motion handler expects events in canvas coordinate space */

		/* first convert from Editor window coordinates to canvas
		 * window coordinates
		 */

		int cx;
		int cy;

		/* clamp x and y to remain within the visible area. except
		 * .. if horizontal scrolling is allowed, always allow us to
		 * move back to zero
		 */

		if (autoscroll_horizontal_allowed) {
			x = min (max ((ArdourCanvas::Coord) x, 0.0), autoscroll_boundary.x1);
		} else {
			x = min (max ((ArdourCanvas::Coord) x, autoscroll_boundary.x0), autoscroll_boundary.x1);
		}
		y = min (max ((ArdourCanvas::Coord) y, autoscroll_boundary.y0), autoscroll_boundary.y1);

		// toplevel->translate_coordinates (*get_canvas_viewport(), x,
		// y, cx, cy);
		cx = x;
		cy = y;

		ArdourCanvas::Duple d = get_canvas()->window_to_canvas (ArdourCanvas::Duple (cx, cy));
		ev.x = d.x;
		ev.y = d.y;
		ev.state = mask;

		motion_handler (0, (GdkEvent*) &ev, true);

	} else {
		stop_canvas_autoscroll ();
		return false;
	}

	autoscroll_cnt++;

	return true; /* call me again */
}

void
CueEditor::start_canvas_autoscroll (bool allow_horiz, bool allow_vert, const ArdourCanvas::Rect& boundary)
{
	if (!_session) {
		return;
	}

	stop_canvas_autoscroll ();

	autoscroll_horizontal_allowed = allow_horiz;
	autoscroll_vertical_allowed = allow_vert;
	autoscroll_boundary = boundary;

	/* do the first scroll right now
	*/

	autoscroll_canvas ();

	/* scroll again at very very roughly 30FPS */

	autoscroll_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &CueEditor::autoscroll_canvas), 30);
}

void
CueEditor::stop_canvas_autoscroll ()
{
	autoscroll_connection.disconnect ();
	autoscroll_cnt = 0;
}

void
CueEditor::visual_changer (const VisualChange& vc)
{
	/**
	 * Changed first so the correct horizontal canvas position is calculated in
	 * EditingContext::set_horizontal_position
	 */
	if (vc.pending & VisualChange::ZoomLevel) {
		set_samples_per_pixel (vc.samples_per_pixel);
	}

	if (vc.pending & VisualChange::TimeOrigin) {
		double new_time_origin = sample_to_pixel_unrounded (vc.time_origin);
		set_horizontal_position (new_time_origin);
		update_rulers ();
	}

	if (vc.pending & VisualChange::YOrigin) {
		vertical_adjustment.set_value (vc.y_origin);
	}

	if (vc.pending & VisualChange::ZoomLevel) {
		if (!(vc.pending & VisualChange::TimeOrigin)) {
			update_rulers ();
		}
	} else {
		/* If the canvas is not being zoomed then the canvas items will not change
		 * and cause Item::prepare_for_render to be called so do it here manually.
		 * Not ideal, but I can't think of a better solution atm.
		 */
		get_canvas()->prepare_for_render();
	}

	/* If we are only scrolling vertically there is no need to update these */
	if (vc.pending != VisualChange::YOrigin) {
		// XXX update_fixed_rulers ();
		redisplay_grid (true);
	}
}

void
CueEditor::catch_pending_show_region ()
{
	if (_visible_pending_region) {
		std::shared_ptr<Region> r (_visible_pending_region);
		_visible_pending_region.reset ();
		set_region (r);
	}
}

Editing::MouseMode
CueEditor::current_mouse_mode () const
{
	return mouse_mode;
}

RegionSelection
CueEditor::region_selection()
{
	RegionSelection rs;
	/* there is never any region-level selection in a pianoroll */
	return rs;
}

void
CueEditor::mouse_mode_toggled (Editing::MouseMode m)
{
	Glib::RefPtr<Gtk::Action>       act  = get_mouse_mode_action (m);
	Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic (act);

	if (!tact->get_active()) {
		/* this was just the notification that the old mode has been
		 * left. we'll get called again with the new mode active in a
		 * jiffy.
		 */
		return;
	}

	mouse_mode = m;

	/* this should generate a new enter event which will
	   trigger the appropriate cursor.
	*/

	if (get_canvas()) {
		get_canvas()->re_enter ();
	}
}

std::pair<Temporal::timepos_t,Temporal::timepos_t>
CueEditor::max_zoom_extent() const
{
	if (_region) {

		Temporal::Beats len;

		if (show_source) {
			len = _region->source()->length().beats();
		} else {
			len = _region->length().beats();
		}

		if (len != Temporal::Beats()) {
			return std::make_pair (Temporal::timepos_t (Temporal::Beats()), Temporal::timepos_t (len));
		}
	}

	/* this needs to match the default empty region length used in ::make_a_region() */
	return std::make_pair (Temporal::timepos_t (Temporal::Beats()), Temporal::timepos_t (Temporal::Beats (32, 0)));
}

void
CueEditor::zoom_to_show (Temporal::timecnt_t const & duration)
{
	if (!_track_canvas_width) {
		zoom_in_allocate = true;
		return;
	}

	reset_zoom ((samplecnt_t) floor (duration.samples() / _track_canvas_width));
}

void
CueEditor::full_zoom_clicked()
{
	/* XXXX NEED LOCAL TEMPO MAP */

	std::pair<Temporal::timepos_t,Temporal::timepos_t> dur (max_zoom_extent());
	samplecnt_t s = dur.second.samples() - dur.first.samples();
	reposition_and_zoom (0,  (s / (double) _visible_canvas_width));
}

void
CueEditor::set_show_source (bool yn)
{
	show_source = yn;
}

void
CueEditor::update_solo_display ()
{
	if (_track->solo_control()->get_value()) {
		solo_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		solo_button.set_active_state (Gtkmm2ext::Off);
	}
}

void
CueEditor::set_track (std::shared_ptr<Track> t)
{
	_track = t;
	_track->solo_control()->Changed.connect (object_connections, invalidator (*this), std::bind (&CueEditor::update_solo_display, this), gui_context());
	update_solo_display ();
}

void
CueEditor::set_region (std::shared_ptr<Region> r)
{
	if (r == _region) {
		return;
	}

	unset (false);

	_region = r;

	if (!get_canvas()->is_visible()) {
		_visible_pending_region = r;
	} else {
		_visible_pending_region.reset ();
	}
}

void
CueEditor::maybe_set_from_rsu ()
{
	RegionUISettingsManager::iterator rsu = ARDOUR_UI::instance()->region_ui_settings_manager.find (_region->id());
	if (rsu != ARDOUR_UI::instance()->region_ui_settings_manager.end()) {
		set_from_rsu (rsu->second);
	}
}

void
CueEditor::set_from_rsu (RegionUISettings& rsu)
{
	follow_playhead_action->set_active (rsu.follow_playhead);

	/* XXXX play selection */

	set_grid_type (rsu.grid_type);
	set_recording_length (rsu.recording_length);
	set_snap_mode (rsu.snap_mode);
	set_zoom_focus (rsu.zoom_focus);
	reposition_and_zoom (rsu.x_origin.samples(), rsu.samples_per_pixel);
	set_recording_length (rsu.recording_length);
	set_draw_length (rsu.draw_length);
	set_draw_velocity (rsu.draw_velocity);
	set_draw_channel (rsu.channel);
}

void
CueEditor::set_trigger (TriggerReference& tref)
{
	if (tref == ref) {
		return;
	}

	_update_connection.disconnect ();
	object_connections.drop_connections ();

	ref = tref;

	Stripable* st = dynamic_cast<Stripable*> (ref.box()->owner());
	assert (st);

	set_track (std::dynamic_pointer_cast<Track> (st->shared_from_this()));
	set_region (ref.trigger()->the_region());

	_update_connection = Timers::rapid_connect (sigc::mem_fun (*this, &CueEditor::maybe_update));
}

void
CueEditor::ruler_locate (GdkEventButton* ev)
{
	if (!_session) {
		return;
	}

	if (ref.box()) {
		/* we don't locate when working with triggers */
		return;
	}

	if (!_region) {
		return;
	}

	samplepos_t sample = pixel_to_sample_from_event (ev->x);
	sample += _region->source_position().samples();
	_session->request_locate (sample);
}

void
CueEditor::maybe_set_count_in ()
{
	if (!ref.box()) {
		std::cerr << "msci no box\n";
		return;
	}

	if (ref.box()->record_enabled() == Disabled) {
		std::cerr << "msci RE\n";
		return;
	}

	count_in_connection.disconnect ();

	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	bool valid;
	count_in_to = ref.box()->start_time (valid);

	if (!valid) {
		std::cerr << "no start time\n";
		return;
	}

	samplepos_t audible (_session->audible_sample());
	Temporal::Beats const & a_q (tmap->quarters_at_sample (audible));

	if ((count_in_to - a_q).get_beats() == 0) {
		std::cerr << "not enough time\n";
		return;
	}

	count_in_connection = ARDOUR_UI::Clock.connect (sigc::bind (sigc::mem_fun (*this, &CueEditor::count_in),  ARDOUR_UI::clock_signal_interval()));
	std::cerr << "count in started" << std::endl;
}

void
CueEditor::count_in (Temporal::timepos_t audible, unsigned int clock_interval_msecs)
{
	if (!_session) {
		return;
	}

	if (!_session->transport_rolling()) {
		return;
	}

	TempoMapPoints grid_points;
	TempoMap::SharedPtr tmap (TempoMap::use());
	Temporal::Beats audible_beats = tmap->quarters_at_sample (audible.samples());
	samplepos_t audible_samples = audible.samples ();

	if (audible_beats >= count_in_to) {
		/* passed the count_in_to time */
		hide_count_in ();
		count_in_connection.disconnect ();
		return;
	}

	tmap->get_grid (grid_points, samples_to_superclock (audible_samples, _session->sample_rate()), samples_to_superclock ((audible_samples + ((_session->sample_rate() / 1000) * clock_interval_msecs)), _session->sample_rate()));

	if (!grid_points.empty()) {

		/* At least one click in the time between now and the next
		 * Clock signal
		 */

		Temporal::Beats current_delta = count_in_to - audible_beats;

		if (current_delta.get_beats() < 1) {
			hide_count_in ();
			count_in_connection.disconnect ();
			return;
		}

		std::string str (string_compose ("%1", current_delta.get_beats()));
		std::cerr << str << std::endl;
		show_count_in (str);
	}
}

bool
CueEditor::ruler_event (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_RELEASE:
		if (ev->button.button == 1) {
			ruler_locate (&ev->button);
		}
		return true;
	default:
		break;
	}

	return false;
}

void
CueEditor::data_captured (samplecnt_t total_duration)
{
	data_capture_duration = total_duration;

	if (!idle_update_queued.exchange (1)) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &CueEditor::idle_data_captured));
	}
}

bool
CueEditor::idle_data_captured ()
{
	if (!ref.box()) {
		return false;
	}

	switch (ref.box()->record_enabled()) {
	case Recording:
		break;
	default:
		return false;
	}

	double where = sample_to_pixel_unrounded (data_capture_duration);

	if (where > _visible_canvas_width * 0.80) {
		set_samples_per_pixel (samples_per_pixel * 1.5);
	}

	idle_update_queued.store (0);
	return false;
}

void
CueEditor::unset (bool trigger_too)
{
	_history.clear ();
	history_connection.disconnect();
	_update_connection.disconnect();
	object_connections.drop_connections ();
	rec_blink_connection.disconnect ();
	count_in_connection.disconnect ();
	capture_connections.drop_connections ();

	_track.reset ();
	_region.reset ();

	if (trigger_too) {
		ref = TriggerReference ();
	}
}

void
CueEditor::session_going_away ()
{
	EditingContext::session_going_away ();
	unset (true);
}

void
CueEditor::load_bindings ()
{
	load_shared_bindings ();
	for (auto & b : bindings) {
		b->associate ();
	}
	set_widget_bindings (*get_canvas(), bindings, Gtkmm2ext::ARDOUR_BINDING_KEY);
}

void
CueEditor::register_actions ()
{
	editor_actions = ActionManager::create_action_group (own_bindings, editor_name());
	bind_mouse_mode_buttons ();
}

ArdourCanvas::GtkCanvasViewport*
CueEditor::get_canvas_viewport() const
{
	return const_cast<ArdourCanvas::GtkCanvasViewport*>(&_canvas_viewport);
}

ArdourCanvas::GtkCanvas*
CueEditor::get_canvas() const
{
	return &_canvas;
}


int
CueEditor::set_state (XMLNode const & node, int version)
{
	set_common_editing_state (node);
	return 0;
}

XMLNode&
CueEditor::get_state () const
{
	XMLNode* node (new XMLNode (editor_name()));
	get_common_editing_state (*node);
	return *node;
}

static void
edit_last_mark_label (std::vector<ArdourCanvas::Ruler::Mark>& marks, const std::string& newlabel)
{
	ArdourCanvas::Ruler::Mark copy = marks.back();
	copy.label = newlabel;
	marks.pop_back ();
	marks.push_back (copy);
}

void
CueEditor::metric_get_bbt (std::vector<ArdourCanvas::Ruler::Mark>& marks, samplepos_t leftmost, samplepos_t rightmost, gint /*maxchars*/)
{
	if (!_session) {
		return;
	}

	bool provided = false;
	std::shared_ptr<Temporal::TempoMap> tmap;
	std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion> (_region);

	if (mr) {
		std::shared_ptr<SMFSource> smf (std::dynamic_pointer_cast<SMFSource> (mr->midi_source()));

		if (smf) {
			tmap = smf->tempo_map (provided);
		}
	}

	if (!provided) {
		tmap.reset (new Temporal::TempoMap (Temporal::Tempo (120, 4), Temporal::Meter (4, 4)));
	}

	EditingContext::TempoMapScope tms (*this, tmap);
	Temporal::TempoMapPoints::const_iterator i;

	char buf[64];
	Temporal::BBT_Time next_beat;
	double bbt_position_of_helper;
	bool helper_active = false;
	ArdourCanvas::Ruler::Mark mark;
	const samplecnt_t sr (_session->sample_rate());

	Temporal::TempoMapPoints grid;
	grid.reserve (4096);

	/* prevent negative values of leftmost from creeping into tempomap
	 */

	const Beats left = tmap->quarters_at_sample (leftmost).round_down_to_beat();
	const Beats lower_beat = (left < Beats() ? Beats() : left);

	using std::max;

	switch (bbt_ruler_scale) {

	case bbt_show_quarters:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 1);
		break;
	case bbt_show_eighths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 2);
		break;
	case bbt_show_sixteenths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 4);
		break;
	case bbt_show_thirtyseconds:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 8);
		break;
	case bbt_show_sixtyfourths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 16);
		break;
	case bbt_show_onetwentyeighths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 32);
		break;

	case bbt_show_1:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 1);
		break;

	case bbt_show_4:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 4);
		break;

	case bbt_show_16:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 16);
		break;

	case bbt_show_64:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 64);
		break;

	default:
		/* bbt_show_many */
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 128);
		break;
	}

#if 0 // DEBUG GRID
	for (auto const& g : grid) {
		std::cout << "Grid " << g.time() <<  " Beats: " << g.beats() << " BBT: " << g.bbt() << " sample: " << g.sample(_session->nominal_sample_rate ()) << "\n";
	}
#endif

	if (distance (grid.begin(), grid.end()) == 0) {
		return;
	}

	/* we can accent certain lines depending on the user's Grid choice */
	/* for example, even in a 4/4 meter we can draw a grid with triplet-feel */
	/* and in this case you will want the accents on '3s' not '2s' */
	uint32_t bbt_divisor = 2;

	using namespace Editing;

	switch (grid_type()) {
	case GridTypeBeatDiv3:
		bbt_divisor = 3;
		break;
	case GridTypeBeatDiv5:
		bbt_divisor = 5;
		break;
	case GridTypeBeatDiv6:
		bbt_divisor = 3;
		break;
	case GridTypeBeatDiv7:
		bbt_divisor = 7;
		break;
	case GridTypeBeatDiv10:
		bbt_divisor = 5;
		break;
	case GridTypeBeatDiv12:
		bbt_divisor = 3;
		break;
	case GridTypeBeatDiv14:
		bbt_divisor = 7;
		break;
	case GridTypeBeatDiv16:
		break;
	case GridTypeBeatDiv20:
		bbt_divisor = 5;
		break;
	case GridTypeBeatDiv24:
		bbt_divisor = 6;
		break;
	case GridTypeBeatDiv28:
		bbt_divisor = 7;
		break;
	case GridTypeBeatDiv32:
		break;
	default:
		bbt_divisor = 2;
		break;
	}

	uint32_t bbt_beat_subdivision = 1;
	switch (bbt_ruler_scale) {
	case bbt_show_quarters:
		bbt_beat_subdivision = 1;
		break;
	case bbt_show_eighths:
		bbt_beat_subdivision = 1;
		break;
	case bbt_show_sixteenths:
		bbt_beat_subdivision = 2;
		break;
	case bbt_show_thirtyseconds:
		bbt_beat_subdivision = 4;
		break;
	case bbt_show_sixtyfourths:
		bbt_beat_subdivision = 8;
		break;
	case bbt_show_onetwentyeighths:
		bbt_beat_subdivision = 16;
		break;
	default:
		bbt_beat_subdivision = 1;
		break;
	}

	bbt_beat_subdivision *= bbt_divisor;

	switch (bbt_ruler_scale) {

	case bbt_show_many:
		snprintf (buf, sizeof(buf), "cannot handle %" PRIu32 " bars", bbt_bars);
		mark.style = ArdourCanvas::Ruler::Mark::Major;
		mark.label = buf;
		mark.position = leftmost;
		marks.push_back (mark);
		break;

	case bbt_show_64:
			for (i = grid.begin(); i != grid.end(); i++) {
				BBT_Time bbt ((*i).bbt());
				if (bbt.is_bar()) {
					if (bbt.bars % 64 == 1) {
						if (bbt.bars % 256 == 1) {
							snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
							mark.style = ArdourCanvas::Ruler::Mark::Major;
						} else {
							buf[0] = '\0';
							if (bbt.bars % 256 == 129)  {
								mark.style = ArdourCanvas::Ruler::Mark::Minor;
							} else {
								mark.style = ArdourCanvas::Ruler::Mark::Micro;
							}
						}
						mark.label = buf;
						mark.position = (*i).sample (sr);
						marks.push_back (mark);
					}
				}
			}
			break;

	case bbt_show_16:
		for (i = grid.begin(); i != grid.end(); i++) {
			BBT_Time bbt ((*i).bbt());
			if (bbt.is_bar()) {
			  if (bbt.bars % 16 == 1) {
				if (bbt.bars % 64 == 1) {
					snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
					mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
					buf[0] = '\0';
					if (bbt.bars % 64 == 33)  {
						mark.style = ArdourCanvas::Ruler::Mark::Minor;
					} else {
						mark.style = ArdourCanvas::Ruler::Mark::Micro;
					}
				}
				mark.label = buf;
				mark.position = (*i).sample(sr);
				marks.push_back (mark);
			  }
			}
		}
	  break;

	case bbt_show_4:
		for (i = grid.begin(); i != grid.end(); ++i) {
			BBT_Time bbt ((*i).bbt());
			if (bbt.is_bar()) {
				if (bbt.bars % 4 == 1) {
					if (bbt.bars % 16 == 1) {
						snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
						mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
						buf[0] = '\0';
						mark.style = ArdourCanvas::Ruler::Mark::Minor;
					}
					mark.label = buf;
					mark.position = (*i).sample (sr);
					marks.push_back (mark);
				}
			}
		}
	  break;

	case bbt_show_1:
		for (i = grid.begin(); i != grid.end(); ++i) {
			BBT_Time bbt ((*i).bbt());
			if (bbt.is_bar()) {
				snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
				mark.style = ArdourCanvas::Ruler::Mark::Major;
				mark.label = buf;
				mark.position = (*i).sample (sr);
				marks.push_back (mark);
			}
		}
	break;

	case bbt_show_quarters:

		mark.label = "";
		mark.position = leftmost;
		mark.style = ArdourCanvas::Ruler::Mark::Micro;
		marks.push_back (mark);

		for (i = grid.begin(); i != grid.end(); ++i) {

			BBT_Time bbt ((*i).bbt());

			if ((*i).sample (sr) < leftmost && (bbt_bar_helper_on)) {
				snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, bbt.bars, bbt.beats);
				edit_last_mark_label (marks, buf);
			} else {

				if (bbt.is_bar()) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
					snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
				} else if ((bbt.beats % 2) == 1) {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
					buf[0] = '\0';
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Micro;
					buf[0] = '\0';
				}
				mark.label = buf;
				mark.position = (*i).sample (sr);
				marks.push_back (mark);
			}
		}
		break;

	case bbt_show_eighths:
	case bbt_show_sixteenths:
	case bbt_show_thirtyseconds:
	case bbt_show_sixtyfourths:
	case bbt_show_onetwentyeighths:

		bbt_position_of_helper = leftmost + (3 * get_current_zoom ());

		mark.label = "";
		mark.position = leftmost;
		mark.style = ArdourCanvas::Ruler::Mark::Micro;
		marks.push_back (mark);

		for (i = grid.begin(); i != grid.end(); ++i) {

			BBT_Time bbt ((*i).bbt());

			if ((*i).sample (sr) < leftmost && (bbt_bar_helper_on)) {
				snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, bbt.bars, bbt.beats);
				edit_last_mark_label (marks, buf);
				helper_active = true;
			} else {

				if (bbt.is_bar()) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
					snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
				} else if (bbt.ticks == 0) {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
					snprintf (buf, sizeof(buf), "%" PRIu32, bbt.beats);
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Micro;
					buf[0] = '\0';
				}

				if (((*i).sample(sr) < bbt_position_of_helper) && helper_active) {
					buf[0] = '\0';
				}
				mark.label =  buf;
				mark.position = (*i).sample (sr);
				marks.push_back (mark);
			}
		}

		break;
	}
}

