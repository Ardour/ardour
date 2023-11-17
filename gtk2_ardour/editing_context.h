/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2011 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2019 Damien Zammit <damien@zamaudio.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
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

#ifndef __ardour_midi_editing_context_h__
#define __ardour_midi_editing_context_h__

#include "pbd/signals.h"

#include "temporal/timeline.h"

#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "widgets/ardour_dropdown.h"

#include "axis_provider.h"
#include "editing.h"
#include "editor_items.h"
#include "selection.h"

using ARDOUR::samplepos_t;
using ARDOUR::samplecnt_t;

class XMLNode;

class CursorContext;
class DragManager;
class EditorCursor;
class MidiRegionView;
class MouseCursors;
class VerboseCursor;
class TrackViewList;
class Selection;
class SelectionMemento;

class EditingContext : public ARDOUR::SessionHandlePtr, public AxisViewProvider
{
public:
	/** Context for mouse entry (stored in a stack). */
	struct EnterContext {
		ItemType                       item_type;
		std::shared_ptr<CursorContext> cursor_ctx;
	};

	EditingContext ();
	~EditingContext ();

	void set_session (ARDOUR::Session*);

	Temporal::TimeDomain time_domain () const;

	DragManager* drags () const {
		return _drags;
	}

	bool drag_active () const;
	bool preview_video_drag_active () const;

	virtual void select_all_within (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, TrackViewList const &, Selection::Operation, bool) = 0;
	virtual void get_per_region_note_selection (std::list<std::pair<PBD::ID, std::set<std::shared_ptr<Evoral::Note<Temporal::Beats> > > > >&) const = 0;
	virtual void get_regionviews_by_id (PBD::ID const id, RegionSelection & regions) const = 0;
	virtual StripableTimeAxisView* get_stripable_time_axis_by_id (const PBD::ID& id) const = 0;
	virtual TrackViewList axis_views_from_routes (std::shared_ptr<ARDOUR::RouteList>) const = 0;

	virtual ARDOUR::Location* find_location_from_marker (ArdourMarker*, bool&) const = 0;
	virtual ArdourMarker* find_marker_from_location_id (PBD::ID const&, bool) const = 0;
	virtual TempoMarker* find_marker_for_tempo (Temporal::TempoPoint const &) = 0;
	virtual MeterMarker* find_marker_for_meter (Temporal::MeterPoint const &) = 0;


	EditorCursor* playhead_cursor () const { return _playhead_cursor; }
	EditorCursor* snapped_cursor () const { return _snapped_cursor; }

	virtual void maybe_autoscroll (bool, bool, bool from_headers) = 0;
	virtual void stop_canvas_autoscroll () = 0;
	virtual bool autoscroll_active() const = 0;

	virtual void redisplay_grid (bool immediate_redraw) = 0;
	virtual Temporal::timecnt_t get_nudge_distance (Temporal::timepos_t const & pos, Temporal::timecnt_t& next) = 0;

	/** Set whether the editor should follow the playhead.
	 * @param yn true to follow playhead, otherwise false.
	 * @param catch_up true to reset the editor view to show the playhead (if yn == true), otherwise false.
	 */
	void set_follow_playhead (bool yn, bool catch_up = true);

	/** Toggle whether the editor is following the playhead */
	void toggle_follow_playhead ();

	/** @return true if the editor is following the playhead */
	bool follow_playhead () const { return _follow_playhead; }

	virtual void instant_save() = 0;

	/** Get the topmost enter context for the given item type.
	 *
	 * This is used to change the cursor associated with a given enter context,
	 * which may not be on the top of the stack.
	 */
	virtual EnterContext* get_enter_context(ItemType type) = 0;

	virtual void begin_selection_op_history () = 0;
	virtual void begin_reversible_selection_op (std::string cmd_name) = 0;
	virtual void commit_reversible_selection_op () = 0;
	virtual void abort_reversible_selection_op () = 0;
	virtual void undo_selection_op () = 0;
	virtual void redo_selection_op () = 0;

	virtual void begin_reversible_command (std::string cmd_name);
	virtual void begin_reversible_command (GQuark);
	virtual void abort_reversible_command ();
	virtual void commit_reversible_command ();

	virtual void set_selected_midi_region_view (MidiRegionView&);

	virtual samplepos_t pixel_to_sample_from_event (double pixel) const = 0;
	virtual samplepos_t pixel_to_sample (double pixel) const = 0;
	virtual double sample_to_pixel (samplepos_t sample) const = 0;
	virtual double sample_to_pixel_unrounded (samplepos_t sample) const = 0;
	virtual double time_to_pixel (Temporal::timepos_t const & pos) const = 0;
	virtual double time_to_pixel_unrounded (Temporal::timepos_t const & pos) const = 0;
	virtual double duration_to_pixels (Temporal::timecnt_t const & pos) const = 0;
	virtual double duration_to_pixels_unrounded (Temporal::timecnt_t const & pos) const = 0;
	/** computes the timeline sample (sample) of an event whose coordinates
	 * are in canvas units (pixels, scroll offset included).
	 */
	virtual samplepos_t canvas_event_sample (GdkEvent const * event, double* pcx = nullptr, double* pcy = nullptr) const = 0;
	/** computes the timeline position for an event whose coordinates
	 * are in canvas units (pixels, scroll offset included). The time
	 * domain used by the return value will match ::default_time_domain()
	 * at the time of calling.
	 */
	virtual Temporal::timepos_t canvas_event_time (GdkEvent const*, double* px = nullptr, double* py = nullptr) const = 0;

	virtual Temporal::Beats get_grid_type_as_beats (bool& success, Temporal::timepos_t const & position) = 0;
	virtual Temporal::Beats get_draw_length_as_beats (bool& success, Temporal::timepos_t const & position) = 0;

	virtual int32_t get_grid_beat_divisions (Editing::GridType gt) = 0;
	virtual int32_t get_grid_music_divisions (Editing::GridType gt, uint32_t event_state) = 0;

	Editing::GridType  grid_type () const;
	bool  grid_type_is_musical (Editing::GridType) const;
	bool  grid_musical () const;

	void cycle_snap_mode ();
	void next_grid_choice ();
	void prev_grid_choice ();
	void set_grid_to (Editing::GridType);
	void set_snap_mode (Editing::SnapMode);

	void set_draw_length_to (Editing::GridType);
	void set_draw_velocity_to (int);
	void set_draw_channel_to (int);

	Editing::GridType  draw_length () const;
	int                draw_velocity () const;
	int                draw_channel () const;

	Editing::SnapMode  snap_mode () const;

	virtual void snap_to (Temporal::timepos_t & first,
	                      Temporal::RoundMode   direction = Temporal::RoundNearest,
	                      ARDOUR::SnapPref      pref = ARDOUR::SnapToAny_Visual,
	                      bool                  ensure_snap = false) = 0;

	virtual void snap_to_with_modifier (Temporal::timepos_t & first,
	                                    GdkEvent const*       ev,
	                                    Temporal::RoundMode   direction = Temporal::RoundNearest,
	                                    ARDOUR::SnapPref      gpref = ARDOUR::SnapToAny_Visual,
	                                    bool ensure_snap = false) = 0;

	virtual Temporal::timepos_t snap_to_bbt (Temporal::timepos_t const & start,
	                                         Temporal::RoundMode   direction,
	                                         ARDOUR::SnapPref    gpref) = 0;

	virtual double get_y_origin () const = 0;
	virtual void reset_x_origin (samplepos_t) = 0;
	virtual void reset_y_origin (double) = 0;

	virtual void set_zoom_focus (Editing::ZoomFocus) = 0;
	virtual Editing::ZoomFocus get_zoom_focus () const = 0;
	virtual samplecnt_t get_current_zoom () const = 0;
	virtual void reset_zoom (samplecnt_t) = 0;
	virtual void reposition_and_zoom (samplepos_t, double) = 0;

	virtual Selection& get_selection() const = 0;
	virtual Selection& get_cut_buffer () const = 0;

	/** Set the mouse mode (gain, object, range, timefx etc.)
	 * @param m Mouse mode (defined in editing_syms.h)
	 * @param force Perform the effects of the change even if no change is required
	 * (ie even if the current mouse mode is equal to @p m)
	 */
	virtual void set_mouse_mode (Editing::MouseMode, bool force = false) = 0;
	/** Step the mouse mode onto the next or previous one.
	 * @param next true to move to the next, otherwise move to the previous
	 */
	virtual void step_mouse_mode (bool next) = 0;
	/** @return The current mouse mode (gain, object, range, timefx etc.)
	 * (defined in editing_syms.h)
	 */
	virtual Editing::MouseMode current_mouse_mode () const = 0;
	/** @return Whether the current mouse mode is an "internal" editing mode. */
	virtual bool internal_editing() const = 0;

	virtual Gdk::Cursor* get_canvas_cursor () const = 0;
	virtual MouseCursors const* cursors () const = 0;
	virtual VerboseCursor* verbose_cursor () const = 0;

	virtual void set_snapped_cursor_position (Temporal::timepos_t const & pos) = 0;

	static sigc::signal<void> DropDownKeys;

	PBD::Signal0<void> SnapChanged;
	PBD::Signal0<void> MouseModeChanged;

	/* MIDI actions, proxied to selected MidiRegionView(s) */
	void midi_action (void (MidiRegionView::*method)());
	virtual std::vector<MidiRegionView*> filter_to_unique_midi_region_views (RegionSelection const & ms) const = 0;

	void register_midi_actions (Gtkmm2ext::Bindings*);

	ArdourCanvas::Rectangle* rubberband_rect;

  protected:
	Glib::RefPtr<Gtk::ActionGroup> _midi_actions;

	/* Cursor stuff.  Do not use directly, use via CursorContext. */
	friend class CursorContext;
	std::vector<Gdk::Cursor*> _cursor_stack;
	virtual void set_canvas_cursor (Gdk::Cursor*) = 0;
	virtual size_t push_canvas_cursor (Gdk::Cursor*) = 0;
	virtual void pop_canvas_cursor () = 0;

	Editing::GridType  pre_internal_grid_type;
	Editing::SnapMode  pre_internal_snap_mode;
	Editing::GridType  internal_grid_type;
	Editing::SnapMode  internal_snap_mode;

	std::vector<std::string> grid_type_strings;

	Glib::RefPtr<Gtk::RadioAction> grid_type_action (Editing::GridType);
	Glib::RefPtr<Gtk::RadioAction> snap_mode_action (Editing::SnapMode);

	Glib::RefPtr<Gtk::RadioAction> draw_length_action (Editing::GridType);
	Glib::RefPtr<Gtk::RadioAction> draw_velocity_action (int);
	Glib::RefPtr<Gtk::RadioAction> draw_channel_action (int);

	Editing::GridType _grid_type;
	Editing::SnapMode _snap_mode;

	Editing::GridType _draw_length;

	int _draw_velocity;
	int _draw_channel;

	ArdourWidgets::ArdourDropdown grid_type_selector;
	void build_grid_type_menu ();

	ArdourWidgets::ArdourDropdown draw_length_selector;
	ArdourWidgets::ArdourDropdown draw_velocity_selector;
	ArdourWidgets::ArdourDropdown draw_channel_selector;
	void build_draw_midi_menus ();

	void grid_type_selection_done (Editing::GridType);
	void snap_mode_selection_done (Editing::SnapMode);
	void snap_mode_chosen (Editing::SnapMode);
	void grid_type_chosen (Editing::GridType);

	void draw_length_selection_done (Editing::GridType);
	void draw_length_chosen (Editing::GridType);

	void draw_velocity_selection_done (int);
	void draw_velocity_chosen (int);

	void draw_channel_selection_done (int);
	void draw_channel_chosen (int);

	DragManager* _drags;

	ArdourWidgets::ArdourButton snap_mode_button;

	virtual void mark_region_boundary_cache_dirty () {}
	virtual void compute_bbt_ruler_scale (samplepos_t, samplepos_t) {}
	virtual void update_tempo_based_rulers () {};
	virtual void show_rulers_for_grid () {};
	virtual samplecnt_t current_page_samples() const = 0;

	samplepos_t       _leftmost_sample;

	/* playhead and edit cursor */

	EditorCursor* _playhead_cursor;
	EditorCursor* _snapped_cursor;

	bool _follow_playhead;
	virtual void reset_x_origin_to_follow_playhead () = 0;

	/* selection process */

	Selection* selection;
	Selection* cut_buffer;
	SelectionMemento* _selection_memento;

	std::list<XMLNode*> before; /* used in *_reversible_command */
};

#endif /* __ardour_midi_editing_context_h__ */

