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

#pragma once

#include "pbd/history_owner.h"

#include "canvas/canvas.h"

#include "widgets/ardour_button.h"
#include "widgets/eventboxext.h"

#include "editing.h"
#include "editing_context.h"
#include "region_ui_settings.h"

namespace Gtk {
	class HScrollbar;
}

class CueEditor : public EditingContext, public PBD::HistoryOwner
{
  public:
	CueEditor (std::string const & name, bool with_transport_controls);
	~CueEditor ();

	virtual Gtk::Widget& contents () = 0;

	void session_going_away ();

	ArdourCanvas::Container* get_trackview_group () const { return data_group; }
	ArdourCanvas::Container* get_noscroll_group() const { return no_scroll_group; }
	ArdourCanvas::ScrollGroup* get_hscroll_group () const { return h_scroll_group; }
	ArdourCanvas::ScrollGroup* get_cursor_scroll_group () const { return cursor_scroll_group; }

	void get_regionviews_by_id (PBD::ID const id, RegionSelection & regions) const;
	StripableTimeAxisView* get_stripable_time_axis_by_id (const PBD::ID& id) const;
	TrackViewList axis_views_from_routes (std::shared_ptr<ARDOUR::RouteList>) const;
	AxisView* axis_view_by_stripable (std::shared_ptr<ARDOUR::Stripable>) const { return nullptr; }
	AxisView* axis_view_by_control (std::shared_ptr<ARDOUR::AutomationControl>) const { return nullptr; }

	ARDOUR::Location* find_location_from_marker (ArdourMarker*, bool&) const;
	ArdourMarker* find_marker_from_location_id (PBD::ID const&, bool) const;
	TempoMarker* find_marker_for_tempo (Temporal::TempoPoint const &);
	MeterMarker* find_marker_for_meter (Temporal::MeterPoint const &);

	void maybe_autoscroll (bool, bool, bool from_headers);
	void stop_canvas_autoscroll ();
	bool autoscroll_active() const;

	void redisplay_grid (bool immediate_redraw);
	Temporal::timecnt_t get_nudge_distance (Temporal::timepos_t const & pos, Temporal::timecnt_t& next) const;
	std::list<SelectableOwner*> selectable_owners() { return std::list<SelectableOwner*>(); }

	void instant_save();

	void begin_selection_op_history ();
	void begin_reversible_selection_op (std::string cmd_name);
	void commit_reversible_selection_op ();
	void abort_reversible_selection_op ();
	void undo_selection_op ();
	void redo_selection_op ();

	RegionSelection region_selection();

	PBD::HistoryOwner& history() { return *this; }
	void history_changed ();
	PBD::ScopedConnection history_connection;

	void add_command (PBD::Command * cmd) { HistoryOwner::add_command (cmd); }
	void add_commands (std::vector<PBD::Command *> cmds) { HistoryOwner::add_commands (cmds); }
	void begin_reversible_command (std::string cmd_name) { HistoryOwner::begin_reversible_command (cmd_name); }
	void begin_reversible_command (GQuark gq) { HistoryOwner::begin_reversible_command (gq); }
	void abort_reversible_command () { HistoryOwner::abort_reversible_command (); }
	void commit_reversible_command () { HistoryOwner::commit_reversible_command (); }

	double get_y_origin () const;

	void set_zoom_focus (Editing::ZoomFocus);
	samplecnt_t get_current_zoom () const;
	virtual void set_samples_per_pixel (samplecnt_t);
	void reposition_and_zoom (samplepos_t, double);

	void set_mouse_mode (Editing::MouseMode, bool force = false);
	/** Step the mouse mode onto the next or previous one.
	 * @param next true to move to the next, otherwise move to the previous
	 */
	void step_mouse_mode (bool next);
	/** @return The current mouse mode (gain, object, range, timefx etc.)
	 * (defined in editing_syms.inc.h)
	 */
	Editing::MouseMode current_mouse_mode () const;
	/** cue editors are *always* used for internal editing */
	bool internal_editing() const { return true; }
	void mouse_mode_toggled (Editing::MouseMode);

	Gdk::Cursor* get_canvas_cursor () const;
	MouseCursors const* cursors () const {
		return _cursors;
	}
	void set_snapped_cursor_position (Temporal::timepos_t const & pos);

	std::vector<MidiRegionView*> filter_to_unique_midi_region_views (RegionSelection const & ms) const;

	std::shared_ptr<Temporal::TempoMap const> start_local_tempo_map (std::shared_ptr<Temporal::TempoMap>);
	void end_local_tempo_map (std::shared_ptr<Temporal::TempoMap const>);

	void scrolled ();
	bool canvas_pre_event (GdkEvent*);
	void catch_pending_show_region ();

	std::pair<Temporal::timepos_t,Temporal::timepos_t> max_zoom_extent() const;

	void full_zoom_clicked();
	void zoom_to_show (Temporal::timecnt_t const &);

	bool ruler_event (GdkEvent*);

	virtual void set_show_source (bool);
	virtual void set_region (std::shared_ptr<ARDOUR::Region>);
	virtual void set_track (std::shared_ptr<ARDOUR::Track>);
	virtual void set_trigger (ARDOUR::TriggerReference&);

	virtual void maybe_update () = 0;

	ArdourCanvas::GtkCanvasViewport* get_canvas_viewport() const;
	ArdourCanvas::GtkCanvas* get_canvas() const;

	int set_state (const XMLNode&, int version);
	XMLNode& get_state () const;

  protected:
	ArdourCanvas::GtkCanvasViewport _canvas_viewport;
	ArdourCanvas::GtkCanvas& _canvas;
	ARDOUR::TriggerReference ref;
	std::shared_ptr<ARDOUR::Region> _region;
	std::shared_ptr<ARDOUR::Track>  _track;
	bool with_transport_controls;
	bool show_source;
	ArdourWidgets::EventBoxExt _contents;
	Gtk::VBox                  _toolbox;
	Gtk::HBox                   button_bar;
	Gtk::HScrollbar*           _canvas_hscrollbar;

	void load_bindings ();
	void register_actions ();

	/* The group containing all other groups that are scrolled vertically
	   and horizontally.
	*/
	ArdourCanvas::ScrollGroup* hv_scroll_group;

	/* The group containing all other groups that are scrolled horizontally ONLY
	*/
	ArdourCanvas::ScrollGroup* h_scroll_group;
	ArdourCanvas::ScrollGroup* v_scroll_group;

	/* Scroll group for cursors, scrolled horizontally, above everything else
	*/
	ArdourCanvas::ScrollGroup* cursor_scroll_group;

	ArdourCanvas::Container* global_rect_group;
	ArdourCanvas::Container* no_scroll_group;
	ArdourCanvas::Container* data_group;

	Gtk::Label length_label;
	Gtk::HBox   rec_box;
	Gtk::HBox   play_box;

	virtual void pack_inner (Gtk::Box&) = 0;
	virtual void pack_outer (Gtk::Box&) = 0;
	void build_zoom_focus_menu ();

	virtual void update_rulers() {}
	virtual bool canvas_enter_leave (GdkEventCrossing* ev) = 0;

	void build_upper_toolbar ();
	void do_undo (uint32_t n);
	void do_redo (uint32_t n);

	Temporal::timepos_t _get_preferred_edit_position (Editing::EditIgnoreOption, bool use_context_click, bool from_outside_canvas);

	ArdourWidgets::ArdourButton rec_enable_button;
	ArdourWidgets::ArdourButton play_button;
	ArdourWidgets::ArdourButton solo_button;
	ArdourWidgets::ArdourButton loop_button;

	ArdourCanvas::Rectangle* transport_loop_range_rect;

	bool play_button_press (GdkEventButton*);
	bool solo_button_press (GdkEventButton*);
	bool bang_button_press (GdkEventButton*);
	bool loop_button_press (GdkEventButton*);

	ArdourWidgets::ArdourDropdown length_selector;
	Temporal::BBT_Offset rec_length;

	bool zoom_in_allocate;

	void set_recording_length (Temporal::BBT_Offset bars);

	bool rec_button_press (GdkEventButton*);
	void rec_enable_change ();
	void blink_rec_enable (bool);
	sigc::connection rec_blink_connection;

	sigc::connection _update_connection;
	PBD::ScopedConnectionList object_connections;

	void trigger_arm_change ();

	double timebar_height;
	size_t n_timebars;

	/* autoscrolling */

	bool autoscroll_canvas ();
	void start_canvas_autoscroll (bool allow_horiz, bool allow_vert, const ArdourCanvas::Rect& boundary);
	void visual_changer (const VisualChange&);

	void update_solo_display ();

	std::shared_ptr<ARDOUR::Region> _visible_pending_region;

	void ruler_locate (GdkEventButton*);

	virtual void begin_write () = 0;
	virtual void end_write () = 0;

	virtual void manage_possible_header (Gtk::Allocation&) {}

	sigc::connection count_in_connection;
	Temporal::Beats count_in_to;

	void count_in (Temporal::timepos_t, unsigned int);
	void maybe_set_count_in ();
	virtual void show_count_in (std::string const &) = 0;
	virtual void hide_count_in () = 0;

	void data_captured (samplecnt_t);
	virtual bool idle_data_captured () = 0;
	std::atomic<int> idle_update_queued;
	PBD::ScopedConnectionList capture_connections;
	samplecnt_t data_capture_duration;

	virtual void unset (bool trigger_too);

	RegionUISettings region_ui_settings;
	void maybe_set_from_rsu ();
	virtual void set_from_rsu (RegionUISettings&);

	void metric_get_bbt (std::vector<ArdourCanvas::Ruler::Mark>&, samplepos_t, samplepos_t, gint);
};

