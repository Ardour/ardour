/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
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

#ifndef __gtk_ardour_public_editor_h__
#define __gtk_ardour_public_editor_h__

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <map>

#include <string>
#include <glib.h>
#include <gdk/gdktypes.h>
#include <gtkmm/box.h>
#include <gtkmm/window.h>
#include <gtkmm/actiongroup.h>
#include <gtkmm/notebook.h>
#include <sigc++/signal.h>

#include "pbd/statefuldestructible.h"
#include "pbd/g_atomic_compat.h"

#include "temporal/beats.h"

#include "evoral/Note.h"

#include "ardour/session_handle.h"

#include "canvas/fwd.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/visibility_tracker.h"

#include "widgets/tabbable.h"

#include "axis_provider.h"
#include "editing.h"
#include "selection.h"

namespace ARDOUR {
	class Session;
	class Region;
	class Playlist;
	class RouteGroup;
	class Trimmable;
	class Movable;
	class Stripable;
}

namespace Gtk {
	class Container;
	class Menu;
}

class AudioRegionView;
class AutomationLine;
class AutomationTimeAxisView;
class ControlPoint;
class DragManager;
class EditorCursor;
class ArdourMarker;
class MeterMarker;
class MixerStrip;
class MouseCursors;
class RegionView;
class RouteTimeAxisView;
class Selection;
class StripableTimeAxisView;
class TempoCurve;
class TempoMarker;
class TimeAxisView;
class VerboseCursor;
struct SelectionRect;

class DisplaySuspender;

namespace ARDOUR_UI_UTILS {
bool relay_key_press (GdkEventKey* ev, Gtk::Window* win);
bool forward_key_press (GdkEventKey* ev);
}

using ARDOUR::samplepos_t;
using ARDOUR::samplecnt_t;

/// Representation of the interface of the Editor class

/** This class contains just the public interface of the Editor class,
 * in order to decouple it from the private implementation, so that callers
 * of PublicEditor need not be recompiled if private methods or member variables
 * change.
 */
class PublicEditor : public ArdourWidgets::Tabbable,  public ARDOUR::SessionHandlePtr, public AxisViewProvider
{
public:
	PublicEditor (Gtk::Widget& content);
	virtual ~PublicEditor ();

	/** @return Singleton PublicEditor instance */
	static PublicEditor& instance () { assert (_instance); return *_instance; }

	virtual bool have_idled() const = 0;
	virtual void first_idle() = 0;

	virtual void setup_tooltips() = 0;

	/* returns the time domain to be used when there's no other overriding
	 * reason to choose one.
	 */
	virtual Temporal::TimeDomain default_time_domain() const = 0;

	/** Attach this editor to a Session.
	 * @param s Session to connect to.
	 */
	virtual void set_session (ARDOUR::Session* s) = 0;

	/** Set the snap type.
	 * @param t Snap type (defined in editing_syms.h)
	 */
	virtual void set_grid_to (Editing::GridType t) = 0;

	virtual Editing::GridType grid_type () const = 0;
	virtual Editing::SnapMode snap_mode () const = 0;

	/** Set the snap mode.
	 * @param m Snap mode (defined in editing_syms.h)
	 */
	virtual void set_snap_mode (Editing::SnapMode m) = 0;

	/**
	 * Snap a value according to the current snap setting.
	 * ensure_snap overrides SnapOff and magnetic snap
	 */
	virtual void snap_to (Temporal::timepos_t & first,
	                      Temporal::RoundMode   direction = Temporal::RoundNearest,
	                      ARDOUR::SnapPref    gpref = ARDOUR::SnapToAny_Visual,
	                      bool                ensure_snap = false) = 0;

	/** Undo some transactions.
	 * @param n Number of transactions to undo.
	 */

	virtual void undo (uint32_t n = 1) = 0;

	/** Redo some transactions.
	 * @param n Number of transaction to redo.
	 */
	virtual void redo (uint32_t n = 1) = 0;

	/** Set the mouse mode (gain, object, range, timefx etc.)
	 * @param m Mouse mode (defined in editing_syms.h)
	 * @param force Perform the effects of the change even if no change is required
	 * (ie even if the current mouse mode is equal to @param m)
	 */
	virtual void set_mouse_mode (Editing::MouseMode m, bool force = false) = 0;

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

	/** Possibly start the audition of a region.
	 *
	 * If \p r is 0, or not an AudioRegion any current audition is cancelled.
	 * If we are currently auditioning \p r , the audition will be cancelled.
	 * Otherwise an audition of \p r will start.
	 *
	 * @param r Region to consider auditioning
	 */
	virtual void consider_auditioning (boost::shared_ptr<ARDOUR::Region> r) = 0;

	/* import dialogs -> ardour-ui ?! */
	virtual void external_audio_dialog () = 0;
	virtual void session_import_dialog () = 0;

	virtual void new_region_from_selection () = 0;
	virtual void separate_region_from_selection () = 0;

	virtual void reverse_region () = 0;
	virtual void normalize_region () = 0;
	virtual void quantize_region () = 0;
	virtual void legatize_region (bool shrink_only) = 0;
	virtual void transform_region () = 0;
	virtual void transpose_region () = 0;
	virtual void pitch_shift_region () = 0;

	virtual void transition_to_rolling (bool fwd) = 0;
	virtual samplepos_t pixel_to_sample (double pixel) const = 0;
	virtual samplepos_t playhead_cursor_sample () const = 0;
	virtual double sample_to_pixel (samplepos_t sample) const = 0;
	virtual double sample_to_pixel_unrounded (samplepos_t sample) const = 0;
	virtual double time_to_pixel (Temporal::timepos_t const &) const = 0;
	virtual double time_to_pixel_unrounded (Temporal::timepos_t const &) const = 0;
	virtual double duration_to_pixels (Temporal::timecnt_t const &) const = 0;
	virtual double duration_to_pixels_unrounded (Temporal::timecnt_t const &) const = 0;

	virtual Selection& get_selection () const = 0;
	virtual bool get_selection_extents (Temporal::timepos_t &start, Temporal::timepos_t &end) const = 0;
	virtual Selection& get_cut_buffer () const = 0;

	virtual void set_selection (std::list<Selectable*>, Selection::Operation) = 0;
	virtual void set_selected_midi_region_view (MidiRegionView&) = 0;

	virtual bool extend_selection_to_track (TimeAxisView&) = 0;
	virtual void play_solo_selection(bool restart) = 0;
	virtual void play_selection () = 0;
	virtual void play_with_preroll () = 0;
	virtual void rec_with_preroll () = 0;
	virtual void rec_with_count_in () = 0;
	virtual void maybe_locate_with_edit_preroll (samplepos_t location) = 0;
	virtual void goto_nth_marker (int nth) = 0;
	virtual void trigger_script (int nth) = 0;
	virtual void add_location_from_playhead_cursor () = 0;
	virtual void remove_location_at_playhead_cursor () = 0;
	virtual void add_location_mark (Temporal::timepos_t const & where) = 0;
	virtual void update_grid () = 0;
	virtual void remove_tracks () = 0;
	virtual void set_loop_range (Temporal::timepos_t const & start, Temporal::timepos_t const & end, std::string cmd) = 0;
	virtual void set_punch_range (Temporal::timepos_t const & start, Temporal::timepos_t const & end, std::string cmd) = 0;

	virtual void jump_forward_to_mark () = 0;
	virtual void jump_backward_to_mark () = 0;

	virtual void set_session_start_from_playhead () = 0;
	virtual void set_session_end_from_playhead () = 0;

	virtual void toggle_location_at_playhead_cursor () = 0;

	virtual void nudge_forward (bool next, bool force_playhead) = 0;
	virtual void nudge_backward (bool next, bool force_playhead) = 0;

	virtual void playhead_forward_to_grid () = 0;
	virtual void playhead_backward_to_grid () = 0;

	virtual void keyboard_selection_begin ( Editing::EditIgnoreOption = Editing::EDIT_IGNORE_NONE) = 0;
	virtual void keyboard_selection_finish (bool add, Editing::EditIgnoreOption = Editing::EDIT_IGNORE_NONE) = 0;

	virtual void set_punch_start_from_edit_point () = 0;
	virtual void set_punch_end_from_edit_point () = 0;
	virtual void set_loop_start_from_edit_point () = 0;
	virtual void set_loop_end_from_edit_point () = 0;

	virtual Editing::MouseMode effective_mouse_mode () const = 0;

	/** Import existing media */
	virtual void do_import (std::vector<std::string> paths, Editing::ImportDisposition, Editing::ImportMode mode, ARDOUR::SrcQuality,
	                        ARDOUR::MidiTrackNameSource, ARDOUR::MidiTempoMapDisposition, Temporal::timepos_t&,
	                        boost::shared_ptr<ARDOUR::PluginInfo> instrument = boost::shared_ptr<ARDOUR::PluginInfo>(),
	                        bool with_markers = false) = 0;
	virtual void do_embed (std::vector<std::string> paths, Editing::ImportDisposition, Editing::ImportMode mode, Temporal::timepos_t&,
	                       boost::shared_ptr<ARDOUR::PluginInfo> instrument = boost::shared_ptr<ARDOUR::PluginInfo>()) = 0;

	/** Open main export dialog */
	virtual void export_audio () = 0;

	/** Open stem export dialog */
	virtual void stem_export () = 0;

	/** Open export dialog with current selection pre-selected */
	virtual void export_selection () = 0;

	/** Open export dialog with current range pre-selected */
	virtual void export_range () = 0;

	virtual void loudness_assistant (bool) = 0;

	virtual void register_actions () = 0;
	virtual void set_zoom_focus (Editing::ZoomFocus) = 0;
	virtual Editing::ZoomFocus get_zoom_focus () const = 0;
	virtual samplecnt_t get_current_zoom () const = 0;
	virtual void reset_zoom (samplecnt_t) = 0;
	virtual void clear_playlist (boost::shared_ptr<ARDOUR::Playlist>) = 0;
	virtual void clear_grouped_playlists (RouteUI*) = 0;

	virtual void mapped_select_playlist_matching (RouteUI&, boost::weak_ptr<ARDOUR::Playlist> pl) = 0;

	virtual void mapover_grouped_routes (sigc::slot<void, RouteUI&> sl, RouteUI*, PBD::PropertyID) const = 0;
	virtual void mapover_armed_routes (sigc::slot<void, RouteUI&> sl) const = 0;
	virtual void mapover_selected_routes (sigc::slot<void, RouteUI&> sl) const = 0;
	virtual void mapover_all_routes (sigc::slot<void, RouteUI&> sl) const = 0;

	virtual void new_playlists_for_all_tracks(bool copy) = 0;
	virtual void new_playlists_for_grouped_tracks(RouteUI* rui, bool copy) = 0;
	virtual void new_playlists_for_selected_tracks(bool copy) = 0;
	virtual void new_playlists_for_armed_tracks(bool copy) = 0;

	virtual void select_all_visible_lanes () = 0;
	virtual void select_all_tracks () = 0;
	virtual void deselect_all () = 0;
	virtual void invert_selection () = 0;
	virtual void set_selected_track (TimeAxisView&, Selection::Operation op = Selection::Set, bool no_remove = false) = 0;
	virtual void set_selected_mixer_strip (TimeAxisView&) = 0;
	virtual void hide_track_in_display (TimeAxisView* tv, bool apply_to_selection = false) = 0;
	virtual void show_track_in_display (TimeAxisView* tv, bool move_into_view = false) = 0;

	virtual void set_stationary_playhead (bool yn) = 0;
	virtual void toggle_stationary_playhead () = 0;
	virtual bool stationary_playhead() const = 0;

	virtual void toggle_cue_behavior () = 0;

	/** Set whether the editor should follow the playhead.
	 * @param yn true to follow playhead, otherwise false.
	 * @param catch_up true to reset the editor view to show the playhead (if yn == true), otherwise false.
	 */
	virtual void set_follow_playhead (bool yn, bool catch_up = true) = 0;

	/** Toggle whether the editor is following the playhead */
	virtual void toggle_follow_playhead () = 0;

	/** @return true if the editor is following the playhead */
	virtual bool follow_playhead () const = 0;

	/** @return true if the playhead is currently being dragged, otherwise false */
	virtual bool dragging_playhead () const = 0;
	virtual samplepos_t leftmost_sample() const = 0;
	virtual samplecnt_t current_page_samples() const = 0;
	virtual double visible_canvas_height () const = 0;
	virtual void temporal_zoom_step (bool coarser) = 0;
	virtual void ensure_time_axis_view_is_visible (TimeAxisView const & tav, bool at_top = false) = 0;
	virtual void override_visible_track_count () = 0;
	virtual void scroll_tracks_down_line () = 0;
	virtual void scroll_tracks_up_line () = 0;
	virtual bool scroll_down_one_track (bool skip_child_views = false) = 0;
	virtual bool scroll_up_one_track (bool skip_child_views = false) = 0;
	virtual void select_topmost_track () = 0;
	virtual void cleanup_regions () = 0;
	virtual void prepare_for_cleanup () = 0;
	virtual void finish_cleanup () = 0;
	virtual void reset_x_origin (samplepos_t sample) = 0;
	virtual double get_y_origin () const = 0;
	virtual void reset_y_origin (double pos) = 0;
	virtual void set_visible_track_count (int32_t) = 0;
	virtual void fit_selection () = 0;
	virtual void remove_last_capture () = 0;
	virtual void maximise_editing_space () = 0;
	virtual void restore_editing_space () = 0;
	virtual Temporal::timepos_t get_preferred_edit_position (Editing::EditIgnoreOption = Editing::EDIT_IGNORE_NONE, bool from_context_menu = false, bool from_outside_canvas = false) = 0;
	virtual void toggle_meter_updating() = 0;
	virtual void split_regions_at (Temporal::timepos_t const &, RegionSelection&) = 0;
	virtual void split_region_at_points (boost::shared_ptr<ARDOUR::Region>, ARDOUR::AnalysisFeatureList&, bool can_ferret, bool select_new = false) = 0;
	virtual void mouse_add_new_marker (Temporal::timepos_t where, ARDOUR::Location::Flags extra_flags = ARDOUR::Location::Flags (0), int32_t cue_id = 0) = 0;
	virtual void foreach_time_axis_view (sigc::slot<void,TimeAxisView&>) = 0;
	virtual void add_to_idle_resize (TimeAxisView*, int32_t) = 0;
	virtual Temporal::timecnt_t get_nudge_distance (Temporal::timepos_t const & pos, Temporal::timecnt_t& next) = 0;
	virtual Temporal::timecnt_t get_paste_offset (Temporal::timepos_t const & pos, unsigned paste_count, Temporal::timecnt_t const & duration) = 0;

	virtual Temporal::Beats get_grid_type_as_beats (bool& success, Temporal::timepos_t const & position) = 0;
	virtual Temporal::Beats get_draw_length_as_beats (bool& success, Temporal::timepos_t const & position) = 0;

	virtual int draw_velocity () const = 0;
	virtual int draw_channel () const = 0;

	virtual unsigned get_grid_beat_divisions (Editing::GridType gt) = 0;
	virtual int32_t get_grid_music_divisions (Editing::GridType gt, uint32_t event_state) = 0;

	virtual void edit_notes (MidiRegionView*) = 0;

	virtual void queue_visual_videotimeline_update () = 0;
	virtual void set_close_video_sensitive (bool) = 0;
	virtual void toggle_ruler_video (bool) = 0;
	virtual void toggle_xjadeo_proc (int) = 0;
	virtual void toggle_xjadeo_viewoption (int, int) = 0;
	virtual void set_xjadeo_sensitive (bool onoff) = 0;
	virtual int  get_videotl_bar_height () const = 0;
	virtual void set_video_timeline_height (const int h) = 0;
	virtual void embed_audio_from_video (std::string, samplepos_t n = 0, bool lock_position_to_video = true) = 0;

	virtual bool track_selection_change_without_scroll () const = 0;
	virtual bool show_touched_automation () const = 0;

	virtual StripableTimeAxisView* get_stripable_time_axis_by_id (const PBD::ID& id) const = 0;

	virtual TimeAxisView* time_axis_view_from_stripable (boost::shared_ptr<ARDOUR::Stripable> s) const = 0;

	virtual void get_equivalent_regions (RegionView* rv, std::vector<RegionView*>&, PBD::PropertyID) const = 0;
	virtual RegionView* regionview_from_region (boost::shared_ptr<ARDOUR::Region>) const = 0;
	virtual RouteTimeAxisView* rtav_from_route (boost::shared_ptr<ARDOUR::Route>) const = 0;

	sigc::signal<void> ZoomChanged;
	sigc::signal<void> Realized;
	sigc::signal<void,samplepos_t> UpdateAllTransportClocks;

	virtual bool pending_locate_request() const = 0;

	static sigc::signal<void> DropDownKeys;

	struct RegionAction {
		Glib::RefPtr<Gtk::Action> action;
		Editing::RegionActionTarget target;

		RegionAction (Glib::RefPtr<Gtk::Action> a, Editing::RegionActionTarget tgt)
			: action (a), target (tgt) {}
	};

	/* data-type of [region] object currently dragged with x-ardour/region.pbdid */
	static ARDOUR::DataType pbdid_dragged_dt;

	std::map<std::string,RegionAction> region_action_map;

	Glib::RefPtr<Gtk::ActionGroup> editor_actions;
	Glib::RefPtr<Gtk::ActionGroup> editor_menu_actions;
	Glib::RefPtr<Gtk::ActionGroup> _region_actions;
	Glib::RefPtr<Gtk::ActionGroup> _midi_actions;

	virtual bool canvas_scroll_event (GdkEventScroll* event, bool from_canvas) = 0;
	virtual bool canvas_control_point_event (GdkEvent* event, ArdourCanvas::Item*, ControlPoint*) = 0;
	virtual bool canvas_line_event (GdkEvent* event, ArdourCanvas::Item*, AutomationLine*) = 0;
	virtual bool canvas_selection_rect_event (GdkEvent* event, ArdourCanvas::Item*, SelectionRect*) = 0;
	virtual bool canvas_selection_start_trim_event (GdkEvent* event, ArdourCanvas::Item*, SelectionRect*) = 0;
	virtual bool canvas_selection_end_trim_event (GdkEvent* event, ArdourCanvas::Item*, SelectionRect*) = 0;
	virtual bool canvas_start_xfade_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*) = 0;
	virtual bool canvas_end_xfade_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*) = 0;
	virtual bool canvas_fade_in_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*) = 0;
	virtual bool canvas_fade_in_handle_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*, bool) = 0;
	virtual bool canvas_fade_out_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*) = 0;
	virtual bool canvas_fade_out_handle_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*, bool) = 0;
	virtual bool canvas_region_view_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_wave_view_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_frame_handle_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_region_view_name_highlight_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_region_view_name_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_feature_line_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_stream_view_event (GdkEvent* event, ArdourCanvas::Item*, RouteTimeAxisView*) = 0;
	virtual bool canvas_marker_event (GdkEvent* event, ArdourCanvas::Item*, ArdourMarker*) = 0;
	virtual bool canvas_videotl_bar_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_tempo_marker_event (GdkEvent* event, ArdourCanvas::Item*, TempoMarker*) = 0;
	virtual bool canvas_tempo_curve_event (GdkEvent* event, ArdourCanvas::Item*, TempoCurve*) = 0;
	virtual bool canvas_meter_marker_event (GdkEvent* event, ArdourCanvas::Item*, MeterMarker*) = 0;
	virtual bool canvas_bbt_marker_event (GdkEvent* event, ArdourCanvas::Item*, BBTMarker*) = 0;
	virtual bool canvas_automation_track_event(GdkEvent* event, ArdourCanvas::Item*, AutomationTimeAxisView*) = 0;

	virtual bool canvas_tempo_bar_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_meter_bar_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_range_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_transport_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_note_event (GdkEvent* event, ArdourCanvas::Item*) = 0;

	static const int window_border_width;
	static const int container_border_width;
	static const int vertical_spacing;
	static const int horizontal_spacing;

	virtual ArdourCanvas::Container* get_trackview_group () const = 0;
	virtual ArdourCanvas::ScrollGroup* get_hscroll_group () const = 0;
	virtual ArdourCanvas::ScrollGroup* get_hvscroll_group () const = 0;
	virtual ArdourCanvas::ScrollGroup* get_cursor_scroll_group () const = 0;
	virtual ArdourCanvas::Container* get_drag_motion_group () const = 0;

	virtual ArdourCanvas::GtkCanvasViewport* get_track_canvas() const = 0;

	virtual void set_current_trimmable (boost::shared_ptr<ARDOUR::Trimmable>) = 0;
	virtual void set_current_movable (boost::shared_ptr<ARDOUR::Movable>) = 0;

	virtual void center_screen (samplepos_t) = 0;

	virtual TrackViewList axis_views_from_routes (boost::shared_ptr<ARDOUR::RouteList>) const = 0;
	virtual TrackViewList const & get_track_views () const = 0;

	virtual MixerStrip* get_current_mixer_strip () const = 0;

	virtual DragManager* drags () const = 0;
	virtual bool drag_active () const = 0;
	virtual bool preview_video_drag_active () const = 0;
	virtual void maybe_autoscroll (bool, bool, bool from_headers) = 0;
	virtual void stop_canvas_autoscroll () = 0;
	virtual bool autoscroll_active() const = 0;

	virtual void begin_reversible_selection_op (std::string cmd_name) = 0;
	virtual void commit_reversible_selection_op () = 0;
	virtual void begin_reversible_command (std::string cmd_name) = 0;
	virtual void begin_reversible_command (GQuark) = 0;
	virtual void abort_reversible_command () = 0;
	virtual void commit_reversible_command () = 0;

	virtual void access_action (const std::string&, const std::string&) = 0;
	virtual void set_toggleaction (const std::string&, const std::string&, bool) = 0;

	virtual MouseCursors const* cursors () const = 0;
	virtual VerboseCursor* verbose_cursor () const = 0;

	virtual EditorCursor* playhead_cursor () const = 0;
	virtual EditorCursor* snapped_cursor () const = 0;

	virtual bool get_smart_mode () const = 0;

	virtual void get_pointer_position (double &, double &) const = 0;

	virtual std::pair <Temporal::timepos_t, Temporal::timepos_t> session_gui_extents (bool use_extra = true) const = 0;

	virtual ARDOUR::Location* find_location_from_marker (ArdourMarker*, bool&) const = 0;
	virtual ArdourMarker* find_marker_from_location_id (PBD::ID const&, bool) const = 0;

	virtual void snap_to_with_modifier (Temporal::timepos_t & first,
	                                    GdkEvent const*      ev,
	                                    Temporal::RoundMode    direction = Temporal::RoundNearest,
	                                    ARDOUR::SnapPref     gpref = ARDOUR::SnapToAny_Visual) = 0;
	virtual Temporal::timepos_t snap_to_bbt (Temporal::timepos_t const & pos, Temporal::RoundMode, ARDOUR::SnapPref) = 0;

	virtual void set_snapped_cursor_position (Temporal::timepos_t const & pos) = 0;

	virtual void get_regions_at (RegionSelection &, Temporal::timepos_t const & where, TrackViewList const &) const = 0;
	virtual void get_regions_after (RegionSelection&, Temporal::timepos_t const & where, const TrackViewList& ts) const = 0;
	virtual RegionSelection get_regions_from_selection_and_mouse (Temporal::timepos_t const &) = 0;
	virtual void get_regionviews_by_id (PBD::ID const id, RegionSelection & regions) const = 0;
	virtual void get_per_region_note_selection (std::list<std::pair<PBD::ID, std::set<boost::shared_ptr<Evoral::Note<Temporal::Beats> > > > >&) const = 0;

	virtual void build_region_boundary_cache () = 0;
	virtual void mark_region_boundary_cache_dirty () = 0;

	virtual void mouse_add_new_tempo_event (Temporal::timepos_t where) = 0;
	virtual void mouse_add_new_meter_event (Temporal::timepos_t where) = 0;
	virtual void edit_tempo_section (Temporal::TempoPoint&) = 0;
	virtual void edit_meter_section (Temporal::MeterPoint&) = 0;

	virtual bool should_ripple () const = 0;

	/// Singleton instance, set up by Editor::Editor()

	static PublicEditor* _instance;

	friend bool ARDOUR_UI_UTILS::relay_key_press (GdkEventKey*, Gtk::Window*);
	friend bool ARDOUR_UI_UTILS::forward_key_press (GdkEventKey*);

	PBD::Signal0<void> SnapChanged;
	PBD::Signal0<void> MouseModeChanged;

	Gtkmm2ext::Bindings* bindings;

protected:
	friend class DisplaySuspender;
	virtual void suspend_route_redisplay () = 0;
	virtual void resume_route_redisplay () = 0;

	GATOMIC_QUAL gint _suspend_route_redisplay_counter;
};

class DisplaySuspender {
	public:
		DisplaySuspender() {
			if (g_atomic_int_add (&PublicEditor::instance()._suspend_route_redisplay_counter, 1) == 0) {
				PublicEditor::instance().suspend_route_redisplay ();
			}
		}
		~DisplaySuspender () {
			if (g_atomic_int_dec_and_test (&PublicEditor::instance()._suspend_route_redisplay_counter)) {
				PublicEditor::instance().resume_route_redisplay ();
			}
		}
};

class MainMenuDisabler {
public:
	MainMenuDisabler () {
		/* The global menu bar continues to be accessible to applications
		   with modal dialogs on mac, which means that we need to desensitize
		   all items in the menu bar.
		*/
		ActionManager::disable_active_actions ();
	}

	~MainMenuDisabler () {
		ActionManager::enable_active_actions ();
	}
};

#endif // __gtk_ardour_public_editor_h__
