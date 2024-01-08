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

#ifndef __gtk_ardour_cue_editor_h__
#define __gtk_ardour_cue_editor_h__

#include "editing.h"
#include "editing_context.h"

class CueEditor : public EditingContext
{
  public:
	CueEditor ();
	~CueEditor ();

	void select_all_within (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, TrackViewList const &, Selection::Operation, bool);

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

	void instant_save();

	/** Get the topmost enter context for the given item type.
	 *
	 * This is used to change the cursor associated with a given enter context,
	 * which may not be on the top of the stack.
	 */
	EnterContext* get_enter_context(ItemType type);

	void begin_selection_op_history ();
	void begin_reversible_selection_op (std::string cmd_name);
	void commit_reversible_selection_op ();
	void abort_reversible_selection_op ();
	void undo_selection_op ();
	void redo_selection_op ();

	double get_y_origin () const;
	void reset_x_origin (samplepos_t);
	void reset_y_origin (double);

	void set_zoom_focus (Editing::ZoomFocus);
	Editing::ZoomFocus get_zoom_focus () const;
	samplecnt_t get_current_zoom () const;
	void reset_zoom (samplecnt_t);
	void reposition_and_zoom (samplepos_t, double);

	void set_mouse_mode (Editing::MouseMode, bool force = false);
	/** Step the mouse mode onto the next or previous one.
	 * @param next true to move to the next, otherwise move to the previous
	 */
	void step_mouse_mode (bool next);
	/** @return The current mouse mode (gain, object, range, timefx etc.)
	 * (defined in editing_syms.h)
	 */
	Editing::MouseMode current_mouse_mode () const;
	/** cue editors are *always* used for internal editing */
	bool internal_editing() const { return true; }

	Gdk::Cursor* get_canvas_cursor () const;
	MouseCursors const* cursors () const {
		return _cursors;
	}
	VerboseCursor* verbose_cursor () const;

	void set_snapped_cursor_position (Temporal::timepos_t const & pos);

	std::vector<MidiRegionView*> filter_to_unique_midi_region_views (RegionSelection const & ms) const;

  protected:
	void set_canvas_cursor (Gdk::Cursor*);
	size_t push_canvas_cursor (Gdk::Cursor*);
	void pop_canvas_cursor ();
	void reset_x_origin_to_follow_playhead ();

};

#endif /* __gtk_ardour_cue_editor_h__ */
