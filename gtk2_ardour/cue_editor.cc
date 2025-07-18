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

#include "widgets/ardour_icon.h"
#include "widgets/tooltips.h"

#include "ardour/types.h"

#include "canvas/canvas.h"

#include "cue_editor.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;

CueEditor::CueEditor (std::string const & name, bool with_transport)
	: EditingContext (name)
	, HistoryOwner (name)
	, with_transport_controls (with_transport)
	, length_label (X_("Record:"))
	, solo_button (S_("Solo|S"))
	, zoom_in_allocate (false)
	, timebar_height (15.)
	, n_timebars (0)
{
	_history.Changed.connect (history_connection, invalidator (*this), std::bind (&CueEditor::history_changed, this), gui_context());
	set_zoom_focus (Editing::ZoomFocusLeft);
}

CueEditor::~CueEditor ()
{
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

Editing::MouseMode
CueEditor::current_mouse_mode () const
{
	return Editing::MouseContent;
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
	_contents.signal_unmap().connect ([this]()  {viewport().unmap (); }, false);
	_contents.signal_map().connect ([this]() { viewport().map (); }, false);
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
CueEditor::play_button_press (GdkEventButton* ev)
{
#warning paul fix lookup region via CueEditor 87
#if 0
	if (_session) {
		_session->request_locate (view->midi_region()->position().samples());
		_session->request_roll ();
	}
#endif
	return true;
}

bool
CueEditor::loop_button_press (GdkEventButton* ev)
{
#warning paul fix region lookup via CueEditor 1
#if 0
	if (!view) {
		return true;
	}
	if (!view->midi_region()) {
		return true;
	}

	if (_session->get_play_loop()) {
		_session->request_play_loop (false);
	} else {
		set_loop_range (view->midi_region()->position(), view->midi_region()->end(), _("loop region"));
		_session->request_play_loop (true);
	}
#endif
	return true;
}

bool
CueEditor::solo_button_press (GdkEventButton* ev)
{
#warning paul fix region lookup via CueEditor 2
#if 0
	if (!view) {
		return true;
	}

	if (!view->midi_track()) {
		return true;
	}

	view->midi_track()->solo_control()->set_value (!view->midi_track()->solo_control()->get_value(), Controllable::NoGroup);
#endif
	return true;
}

bool
CueEditor::rec_button_press (GdkEventButton* ev)
{
#warning paul fix trigger lookup via CueEditor 1
#if 0
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
#endif
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
#warning paul fix trigger lookup via CueEditor 1
#if 0
	if (!ref.trigger()) {
		return;
	}

	if (!ref.trigger()->armed()) {
		view->end_write ();
	} else {
		maybe_set_count_in ();
	}
#endif
	rec_enable_change ();
}

void
CueEditor::rec_enable_change ()
{
#warning paul fix trigger lookup via CueEditor 1
#if 0
	if (!ref.box()) {
		std::cerr << "no box!\n";
		return;
	}

	rec_blink_connection.disconnect ();
	count_in_connection.disconnect ();

	std::cerr << "REC, state " << ref.box()->record_enabled() << std::endl;

	switch (ref.box()->record_enabled()) {
	case Recording:
		rec_enable_button.set_active_state (Gtkmm2ext::ExplicitActive);
		rec_blink_connection.disconnect ();
		if (view) {
			view->begin_write ();
		}
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
#endif
}

void
CueEditor::set_recording_length (Temporal::BBT_Offset dur)
{
	rec_length = dur;
}

bool
CueEditor::bang_button_press (GdkEventButton* ev)
{
#warning paul fix trigger look from CueEditor 93
#if 0
	if (!ref.trigger()) {
		return true;
	}

	ref.trigger()->bang ();
#endif
	return true;
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

#warning paul fix use of PRH in CueEditor context
#if 0
		if (prh) {
			double w, h;
			prh->size_request (w, h);

			alloc.set_width (alloc.get_width() - w);
			alloc.set_x (alloc.get_x() + w);
		}
#endif
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
	Gtk::Window* toplevel = dynamic_cast<Gtk::Window*> (viewport().get_toplevel());

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
		set_region (_visible_pending_region);
		_visible_pending_region.reset ();
	}
}
