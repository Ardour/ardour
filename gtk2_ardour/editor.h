/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
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

#ifndef __ardour_editor_h__
#define __ardour_editor_h__

#include <sys/time.h>

#include <cmath>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <boost/optional.hpp>

#include <gtkmm/comboboxtext.h>
#include <gtkmm/layout.h>

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/dndtreeview.h"

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/import_status.h"
#include "ardour/tempo.h"
#include "ardour/location.h"
#include "ardour/types.h"

#include "canvas/fwd.h"
#include "canvas/ruler.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/pane.h"

#include "ardour_dialog.h"
#include "public_editor.h"
#include "editing.h"
#include "enums.h"
#include "editor_items.h"
#include "region_selection.h"
#include "selection_memento.h"
#include "tempo_curve.h"

#include "ptformat/ptformat.h"

namespace Gtkmm2ext {
	class Bindings;
}

namespace Evoral {
	class SMF;
}

namespace ARDOUR {
	class AudioPlaylist;
	class AudioRegion;
	class AudioTrack;
	class ChanCount;
	class Filter;
	class Location;
	class MidiOperator;
	class MidiRegion;
	class MidiTrack;
	class Playlist;
	class Region;
	class RouteGroup;
	class Session;
	class Track;
}

namespace LADSPA {
	class Plugin;
}

class AnalysisWindow;
class AudioClock;
class AudioRegionView;
class AudioStreamView;
class AudioTimeAxisView;
class AutomationLine;
class AutomationSelection;
class AutomationTimeAxisView;
class BundleManager;
class ControlPoint;
class CursorContext;
class DragManager;
class EditNoteDialog;
class EditorCursor;
class EditorGroupTabs;
class EditorLocations;
class EditorRegions;
class EditorSources;
class EditorRoutes;
class EditorRouteGroups;
class EditorSnapshots;
class EditorSummary;
class GUIObjectState;
class ArdourMarker;
class MidiRegionView;
class MidiExportDialog;
class MixerStrip;
class MouseCursors;
class NoteBase;
class PluginSelector;
class ProgressReporter;
class QuantizeDialog;
class RegionPeakCursor;
class RhythmFerret;
class RulerDialog;
class Selection;
class SelectionPropertiesBox;
class SoundFileOmega;
class StreamView;
class GridLines;
class TempoLines;
class TimeAxisView;
class TimeInfoBox;
class TimeFXDialog;
class TimeSelection;
class RegionLayeringOrderEditor;
class VerboseCursor;

class Editor : public PublicEditor, public PBD::ScopedConnectionList
{
public:
	Editor ();
	~Editor ();

	void             set_session (ARDOUR::Session*);

	Gtk::Window* use_own_window (bool and_fill_it);

	void             first_idle ();
	virtual bool     have_idled () const { return _have_idled; }

	bool pending_locate_request() const { return _pending_locate_request; }

	Temporal::TimeDomain default_time_domain() const;

	samplepos_t leftmost_sample() const { return _leftmost_sample; }

	samplecnt_t current_page_samples() const {
		return (samplecnt_t) _visible_canvas_width* samples_per_pixel;
	}

	double visible_canvas_height () const {
		return _visible_canvas_height;
	}
	double trackviews_height () const;

	void cycle_snap_mode ();
	void next_grid_choice ();
	void prev_grid_choice ();
	void set_grid_to (Editing::GridType);
	void set_snap_mode (Editing::SnapMode);

	void set_draw_length_to (Editing::GridType);
	void set_draw_velocity_to (int);
	void set_draw_channel_to (int);

	Editing::SnapMode  snap_mode () const;
	Editing::GridType  grid_type () const;
	bool  grid_type_is_musical (Editing::GridType) const;
	bool  grid_musical () const;

	bool on_velocity_scroll_event (GdkEventScroll*);

	Editing::GridType  draw_length () const;
	int                draw_velocity () const;
	int                draw_channel () const;

	void undo (uint32_t n = 1);
	void redo (uint32_t n = 1);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	void set_mouse_mode (Editing::MouseMode, bool force = false);
	void step_mouse_mode (bool next);
	Editing::MouseMode current_mouse_mode () const { return mouse_mode; }
	Editing::MidiEditMode current_midi_edit_mode () const;
	void remove_midi_note (ArdourCanvas::Item*, GdkEvent*);

	bool internal_editing() const;

	void foreach_time_axis_view (sigc::slot<void,TimeAxisView&>);
	void add_to_idle_resize (TimeAxisView*, int32_t);

	StripableTimeAxisView* get_stripable_time_axis_by_id (const PBD::ID& id) const;

	void consider_auditioning (boost::shared_ptr<ARDOUR::Region>);
	void hide_a_region (boost::shared_ptr<ARDOUR::Region>);
	void show_a_region (boost::shared_ptr<ARDOUR::Region>);

#ifdef USE_RUBBERBAND
	std::vector<std::string> rb_opt_strings;
	int rb_current_opt;
#endif

	/* things that need to be public to be used in the main menubar */

	void new_region_from_selection ();
	void separate_regions_between (const TimeSelection&);
	void separate_region_from_selection ();
	void separate_under_selected_regions ();
	void separate_region_from_punch ();
	void separate_region_from_loop ();
	void separate_regions_using_location (ARDOUR::Location&);
	void transition_to_rolling (bool forward);

	/* NOTE: these functions assume that the "pixel" coordinate is
	   in canvas coordinates. These coordinates already take into
	   account any scrolling offsets.
	*/

	samplepos_t pixel_to_sample_from_event (double pixel) const {

		/* pixel can be less than zero when motion events
		   are processed. since we've already run the world->canvas
		   affine, that means that the location *really* is "off
		   to the right" and thus really is "before the start".
		*/

		if (pixel >= 0) {
			return pixel * samples_per_pixel;
		} else {
			return 0;
		}
	}

	samplepos_t pixel_to_sample (double pixel) const {
		return pixel * samples_per_pixel;
	}

	double sample_to_pixel (samplepos_t sample) const {
		return round (sample / (double) samples_per_pixel);
	}

	double sample_to_pixel_unrounded (samplepos_t sample) const {
		return sample / (double) samples_per_pixel;
	}

	double time_to_pixel (Temporal::timepos_t const & pos) const;
	double time_to_pixel_unrounded (Temporal::timepos_t const & pos) const;

	double duration_to_pixels (Temporal::timecnt_t const & pos) const;
	double duration_to_pixels_unrounded (Temporal::timecnt_t const & pos) const;

	/* selection */

	Selection& get_selection() const { return *selection; }
	bool get_selection_extents (Temporal::timepos_t &start, Temporal::timepos_t &end) const;  // the time extents of the current selection, whether Range, Region(s), Control Points, or Notes
	Selection& get_cut_buffer() const { return *cut_buffer; }

	void get_regionviews_at_or_after (Temporal::timepos_t const &, RegionSelection&);

	void set_selection (std::list<Selectable*>, Selection::Operation);
	void set_selected_midi_region_view (MidiRegionView&);

	bool extend_selection_to_track (TimeAxisView&);

	void play_selection ();
	void maybe_locate_with_edit_preroll (samplepos_t);
	void play_with_preroll ();
	void rec_with_preroll ();
	void rec_with_count_in ();
	void select_all_in_track (Selection::Operation op);
	void select_all_objects (Selection::Operation op);
	void invert_selection_in_track ();
	void invert_selection ();
	void deselect_all ();
	long select_range (Temporal::timepos_t const & , Temporal::timepos_t const &);

	void set_selected_regionview_from_region_list (boost::shared_ptr<ARDOUR::Region> region, Selection::Operation op = Selection::Set);

	void remove_tracks ();

	/* tempo */

	void update_grid ();

	/* analysis window */

	void loudness_analyze_region_selection();
	void loudness_analyze_range_selection();

	void spectral_analyze_region_selection();
	void spectral_analyze_range_selection();

	/* export */

	void export_audio ();
	void stem_export ();
	void export_selection ();
	void export_range ();
	void export_region ();

	/* export for analysis only */
	void loudness_assistant (bool);
	void loudness_assistant_marker ();
	void measure_master_loudness (samplepos_t start, samplepos_t end, bool);

	bool process_midi_export_dialog (MidiExportDialog& dialog, boost::shared_ptr<ARDOUR::MidiRegion> midi_region);

	void               set_zoom_focus (Editing::ZoomFocus);
	Editing::ZoomFocus get_zoom_focus () const { return zoom_focus; }
	samplecnt_t        get_current_zoom () const { return samples_per_pixel; }
	void               cycle_zoom_focus ();
	void temporal_zoom_step (bool zoom_out);
	void temporal_zoom_step_scale (bool zoom_out, double scale);
	void temporal_zoom_step_mouse_focus (bool zoom_out);
	void temporal_zoom_step_mouse_focus_scale (bool zoom_out, double scale);
	void ensure_time_axis_view_is_visible (TimeAxisView const & tav, bool at_top);
	void tav_zoom_step (bool coarser);
	void tav_zoom_smooth (bool coarser, bool force_all);

	/* stuff that AudioTimeAxisView and related classes use */

	void clear_playlist (boost::shared_ptr<ARDOUR::Playlist>);

	void clear_grouped_playlists (RouteUI* v);

	void get_onscreen_tracks (TrackViewList&);

	Width editor_mixer_strip_width;
	void maybe_add_mixer_strip_width (XMLNode&);
	void show_editor_mixer (bool yn);
	void create_editor_mixer ();
	void show_editor_list (bool yn);
	void set_selected_mixer_strip (TimeAxisView&);
	void mixer_strip_width_changed ();
	void hide_track_in_display (TimeAxisView* tv, bool apply_to_selection = false);
	void show_track_in_display (TimeAxisView* tv, bool move_into_view = false);
	void tempo_curve_selected (Temporal::TempoPoint const * ts, bool yn);

	/* nudge is initiated by transport controls owned by ARDOUR_UI */

	Temporal::timecnt_t get_nudge_distance (Temporal::timepos_t const & pos, Temporal::timecnt_t& next);
	Temporal::timecnt_t get_paste_offset (Temporal::timepos_t const & pos, unsigned paste_count, Temporal::timecnt_t const & duration);

	Temporal::Beats get_grid_type_as_beats (bool& success, Temporal::timepos_t const & position);
	Temporal::Beats get_draw_length_as_beats (bool& success, Temporal::timepos_t const & position);

	unsigned get_grid_beat_divisions (Editing::GridType gt);
	int32_t get_grid_music_divisions (Editing::GridType gt, uint32_t event_state);

	void nudge_forward (bool next, bool force_playhead);
	void nudge_backward (bool next, bool force_playhead);

	/* nudge initiated from context menu */

	void nudge_forward_capture_offset ();
	void nudge_backward_capture_offset ();

	void sequence_regions ();

	/* playhead/screen stuff */

	void set_stationary_playhead (bool yn);
	void toggle_stationary_playhead ();
	bool stationary_playhead() const { return _stationary_playhead; }

	void set_follow_playhead (bool yn, bool catch_up = true);
	void toggle_follow_playhead ();
	bool follow_playhead() const { return _follow_playhead; }
	bool dragging_playhead () const { return _dragging_playhead; }

	void toggle_zero_line_visibility ();
	void set_summary ();
	void set_group_tabs ();

	/* returns the left-most and right-most time that the gui should allow the user to scroll to */
	std::pair <Temporal::timepos_t,Temporal::timepos_t> session_gui_extents (bool use_extra = true) const;

	/* RTAV Automation display option */
	bool show_touched_automation () const;

	/* fades */

	void toggle_region_fades (int dir);
	void update_region_fade_visibility ();

	/* floating windows/transient */

	void ensure_float (Gtk::Window&);

	void scroll_tracks_down_line ();
	void scroll_tracks_up_line ();

	bool scroll_up_one_track (bool skip_child_views = false);
	bool scroll_down_one_track (bool skip_child_views = false);

	void scroll_left_step ();
	void scroll_right_step ();

	void scroll_left_half_page ();
	void scroll_right_half_page ();

	void select_topmost_track ();

	void cleanup_regions ();

	void prepare_for_cleanup ();
	void finish_cleanup ();

	void maximise_editing_space();
	void restore_editing_space();

	double get_y_origin () const;
	void reset_x_origin (samplepos_t);
	void reset_x_origin_to_follow_playhead ();
	void reset_y_origin (double);
	void reset_zoom (samplecnt_t);
	void reposition_and_zoom (samplepos_t, double);

	Temporal::timepos_t get_preferred_edit_position (Editing::EditIgnoreOption = Editing::EDIT_IGNORE_NONE,
	                                                 bool use_context_click = false,
	                                                 bool from_outside_canvas = false);

	bool update_mouse_speed ();
	bool decelerate_mouse_speed ();

	void toggle_meter_updating();

	void show_rhythm_ferret();

	void goto_visual_state (uint32_t);
	void save_visual_state (uint32_t);

	void queue_draw_resize_line (int at);
	void start_resize_line_ops ();
	void end_resize_line_ops ();

	TrackViewList const & get_track_views () const {
		return track_views;
	}

	void do_ptimport(std::string path, ARDOUR::SrcQuality quality);

	void do_import (std::vector<std::string>              paths,
	                Editing::ImportDisposition            disposition,
	                Editing::ImportMode                   mode,
	                ARDOUR::SrcQuality                    quality,
	                ARDOUR::MidiTrackNameSource           mts,
	                ARDOUR::MidiTempoMapDisposition       mtd,
	                Temporal::timepos_t&                  pos,
	                boost::shared_ptr<ARDOUR::PluginInfo> instrument = boost::shared_ptr<ARDOUR::PluginInfo>(),
	                bool with_markers = false);

	void do_embed (std::vector<std::string>              paths,
	               Editing::ImportDisposition            disposition,
	               Editing::ImportMode                   mode,
	               Temporal::timepos_t&                  pos,
	               boost::shared_ptr<ARDOUR::PluginInfo> instrument = boost::shared_ptr<ARDOUR::PluginInfo>());

	void get_regionview_corresponding_to (boost::shared_ptr<ARDOUR::Region> region, std::vector<RegionView*>& regions);

	void get_regionviews_by_id (PBD::ID const id, RegionSelection & regions) const;
	void get_per_region_note_selection (std::list<std::pair<PBD::ID, std::set<boost::shared_ptr<Evoral::Note<Temporal::Beats> > > > >&) const;

	void center_screen (samplepos_t);

	TrackViewList axis_views_from_routes (boost::shared_ptr<ARDOUR::RouteList>) const;

	void snap_to (Temporal::timepos_t & first,
	              Temporal::RoundMode    direction = Temporal::RoundNearest,
	              ARDOUR::SnapPref     pref = ARDOUR::SnapToAny_Visual,
	              bool                 ensure_snap = false);

	void snap_to_with_modifier (Temporal::timepos_t & first,
	                            GdkEvent const*      ev,
	                            Temporal::RoundMode    direction = Temporal::RoundNearest,
	                            ARDOUR::SnapPref     gpref = ARDOUR::SnapToAny_Visual);
	Temporal::timepos_t snap_to_bbt (Temporal::timepos_t const & start,
	                                 Temporal::RoundMode   direction,
	                                 ARDOUR::SnapPref    gpref);

	void set_snapped_cursor_position (Temporal::timepos_t const & pos);

	void begin_selection_op_history ();
	void begin_reversible_selection_op (std::string cmd_name);
	void commit_reversible_selection_op ();
	void undo_selection_op ();
	void redo_selection_op ();
	void begin_reversible_command (std::string cmd_name);
	void begin_reversible_command (GQuark);
	void abort_reversible_command ();
	void commit_reversible_command ();

	MixerStrip* get_current_mixer_strip () const {
		return current_mixer_strip;
	}

	DragManager* drags () const {
		return _drags;
	}

	bool drag_active () const;
	bool preview_video_drag_active () const;

	void maybe_autoscroll (bool, bool, bool);
	bool autoscroll_active() const;

	Gdk::Cursor* get_canvas_cursor () const;

	void set_current_trimmable (boost::shared_ptr<ARDOUR::Trimmable>);
	void set_current_movable (boost::shared_ptr<ARDOUR::Movable>);

	MouseCursors const* cursors () const {
		return _cursors;
	}

	VerboseCursor* verbose_cursor () const {
		return _verbose_cursor;
	}

	double clamp_verbose_cursor_x (double);
	double clamp_verbose_cursor_y (double);

	void get_pointer_position (double &, double &) const;

	/** Context for mouse entry (stored in a stack). */
	struct EnterContext {
		ItemType                         item_type;
		boost::shared_ptr<CursorContext> cursor_ctx;
	};

	/** Get the topmost enter context for the given item type.
	 *
	 * This is used to change the cursor associated with a given enter context,
	 * which may not be on the top of the stack.
	 */
	EnterContext* get_enter_context(ItemType type);

	TimeAxisView* stepping_axis_view () {
		return _stepping_axis_view;
	}

	void set_stepping_axis_view (TimeAxisView* v) {
		_stepping_axis_view = v;
	}

	ArdourCanvas::Container* get_trackview_group () const { return _trackview_group; }
	ArdourCanvas::Container* get_noscroll_group () const { return no_scroll_group; }
	ArdourCanvas::ScrollGroup* get_hscroll_group () const { return h_scroll_group; }
	ArdourCanvas::ScrollGroup* get_hvscroll_group () const { return hv_scroll_group; }
	ArdourCanvas::ScrollGroup* get_cursor_scroll_group () const { return cursor_scroll_group; }
	ArdourCanvas::Container* get_drag_motion_group () const { return _drag_motion_group; }

	ArdourCanvas::GtkCanvasViewport* get_track_canvas () const;

	void override_visible_track_count ();

	/* Ruler metrics methods */

	void metric_get_timecode (std::vector<ArdourCanvas::Ruler::Mark>&, int64_t, int64_t, gint);
	void metric_get_bbt (std::vector<ArdourCanvas::Ruler::Mark>&, int64_t, int64_t, gint);
	void metric_get_samples (std::vector<ArdourCanvas::Ruler::Mark>&, int64_t, int64_t, gint);
	void metric_get_minsec (std::vector<ArdourCanvas::Ruler::Mark>&, int64_t, int64_t, gint);

	/* editing operations that need to be public */
	void mouse_add_new_marker (Temporal::timepos_t where, ARDOUR::Location::Flags extra_flags = ARDOUR::Location::Flags (0), int32_t cue_id = 0);
	void split_regions_at (Temporal::timepos_t const & , RegionSelection&);
	void split_region_at_points (boost::shared_ptr<ARDOUR::Region>, ARDOUR::AnalysisFeatureList&, bool can_ferret, bool select_new = false);
	RegionSelection get_regions_from_selection_and_mouse (Temporal::timepos_t const &);
	void do_remove_gaps ();
	void remove_gaps (Temporal::timecnt_t const & threshold, Temporal::timecnt_t const & leave, bool markers_too);

	void mouse_brush_insert_region (RegionView*, Temporal::timepos_t const & pos);

	void mouse_add_new_tempo_event (Temporal::timepos_t where);
	void mouse_add_new_meter_event (Temporal::timepos_t where);
	void edit_tempo_section (Temporal::TempoPoint&);
	void edit_meter_section (Temporal::MeterPoint&);

	bool should_ripple () const;
	void do_ripple (boost::shared_ptr<ARDOUR::Playlist>, Temporal::timepos_t const &, Temporal::timecnt_t const &, ARDOUR::RegionList* exclude, bool add_to_command);
	void do_ripple (boost::shared_ptr<ARDOUR::Playlist>, Temporal::timepos_t const &, Temporal::timecnt_t const &, boost::shared_ptr<ARDOUR::Region> exclude, bool add_to_command);
	void ripple_marks (boost::shared_ptr<ARDOUR::Playlist> target_playlist, Temporal::timepos_t at, Temporal::timecnt_t const & distance);
	void get_markers_to_ripple (boost::shared_ptr<ARDOUR::Playlist> target_playlist, Temporal::timepos_t const & pos, std::vector<ArdourMarker*>& markers);
	Temporal::timepos_t effective_ripple_mark_start (boost::shared_ptr<ARDOUR::Playlist> target_playlist, Temporal::timepos_t pos);

	void add_region_marker ();
	void clear_region_markers ();
	void remove_region_marker (ARDOUR::CueMarker&);
	void make_region_markers_global (bool as_cd_markers);

protected:
	void map_transport_state ();
	void map_position_change (samplepos_t);
	void transport_looped ();

	void on_realize();

	void suspend_route_redisplay ();
	void resume_route_redisplay ();

private:

	void color_handler ();

	bool                 constructed;

	// to keep track of the playhead position for control_scroll
	boost::optional<samplepos_t> _control_scroll_target;

	SelectionPropertiesBox*      _properties_box;

	typedef std::pair<TimeAxisView*,XMLNode*> TAVState;

	struct VisualState {
		VisualState (bool with_tracks);
		~VisualState ();
		double              y_position;
		samplecnt_t         samples_per_pixel;
		samplepos_t        _leftmost_sample;
		Editing::ZoomFocus  zoom_focus;
		GUIObjectState*     gui_state;
	};

	std::list<VisualState*> undo_visual_stack;
	std::list<VisualState*> redo_visual_stack;
	VisualState* current_visual_state (bool with_tracks = true);
	void undo_visual_state ();
	void redo_visual_state ();
	void use_visual_state (VisualState&);
	bool no_save_visual;
	void swap_visual_state ();

	std::vector<VisualState*> visual_states;
	void start_visual_state_op (uint32_t n);
	void cancel_visual_state_op (uint32_t n);

	samplepos_t       _leftmost_sample;
	samplecnt_t        samples_per_pixel;
	Editing::ZoomFocus zoom_focus;

	void set_samples_per_pixel (samplecnt_t);
	void on_samples_per_pixel_changed ();

	Editing::MouseMode mouse_mode;
	Editing::GridType  pre_internal_grid_type;
	Editing::SnapMode  pre_internal_snap_mode;
	Editing::GridType  internal_grid_type;
	Editing::SnapMode  internal_snap_mode;
	Editing::MouseMode effective_mouse_mode () const;

	enum JoinObjectRangeState {
		JOIN_OBJECT_RANGE_NONE,
		/** `join object/range' mode is active and the mouse is over a place where object mode should happen */
		JOIN_OBJECT_RANGE_OBJECT,
		/** `join object/range' mode is active and the mouse is over a place where range mode should happen */
		JOIN_OBJECT_RANGE_RANGE
	};

	JoinObjectRangeState _join_object_range_state;

	void update_join_object_range_location (double);

	boost::optional<float>  pre_notebook_shrink_pane_width;

	Gtk::VBox _editor_list_vbox;
	Gtk::Notebook _the_notebook;
	bool _notebook_shrunk;
	void add_notebook_page (std::string const&, Gtk::Widget&);
	bool notebook_tab_clicked (GdkEventButton*, Gtk::Widget*);

	ArdourWidgets::HPane edit_pane;
	ArdourWidgets::VPane editor_summary_pane;

	Gtk::EventBox meter_base;
	Gtk::EventBox marker_base;
	Gtk::HBox     marker_box;
	Gtk::VBox     scrollers_rulers_markers_box;

	void location_changed (ARDOUR::Location*);
	void location_flags_changed (ARDOUR::Location*);
	void refresh_location_display ();
	void refresh_location_display_internal (const ARDOUR::Locations::LocationList&);
	void add_new_location (ARDOUR::Location*);
	ArdourCanvas::Container* add_new_location_internal (ARDOUR::Location*);
	void location_gone (ARDOUR::Location*);
	void remove_marker (ArdourCanvas::Item&);
	void remove_marker (ArdourMarker*);
	gint really_remove_global_marker (ARDOUR::Location* loc);
	gint really_remove_region_marker (ArdourMarker*);
	void goto_nth_marker (int nth);
	void trigger_script (int nth);
	void trigger_script_by_name (const std::string script_name);
	void toggle_marker_lines ();
	void set_marker_line_visibility (bool);

	void jump_forward_to_mark ();
	void jump_backward_to_mark ();

	uint32_t location_marker_color;
	uint32_t location_range_color;
	uint32_t location_loop_color;
	uint32_t location_punch_color;
	uint32_t location_cd_marker_color;

	struct LocationMarkers {
		ArdourMarker* start;
		ArdourMarker* end;
		bool    valid;

		LocationMarkers () : start(0), end(0), valid (true) {}

		~LocationMarkers ();

		void hide ();
		void show ();

		void set_show_lines (bool);
		void set_selected (bool);
		void set_entered (bool);
		void setup_lines ();

		void set_name (const std::string&);
		void set_position (Temporal::timepos_t const & start, Temporal::timepos_t const & end = Temporal::timepos_t());
		void set_color_rgba (uint32_t);
	};

	LocationMarkers*  find_location_markers (ARDOUR::Location*) const;
	ARDOUR::Location* find_location_from_marker (ArdourMarker*, bool& is_start) const;
	ArdourMarker* find_marker_from_location_id (PBD::ID const&, bool) const;
	ArdourMarker* entered_marker;
	bool _show_marker_lines;

	typedef std::map<ARDOUR::Location*,LocationMarkers*> LocationMarkerMap;
	LocationMarkerMap location_markers;

	void update_marker_labels ();
	void update_marker_labels (ArdourCanvas::Item*);
	void check_marker_label (ArdourMarker*);

	/** A set of lists of Markers that are in each of the canvas groups
	 *  for the marker sections at the top of the editor.  These lists
	 *  are kept sorted in time order between marker movements, so that after
	 *  a marker has moved we can decide whether we need to update the labels
	 *  for all markers or for just a few.
	 */
	std::map<ArdourCanvas::Item*, std::list<ArdourMarker*> > _sorted_marker_lists;
	void remove_sorted_marker (ArdourMarker*);

	void hide_marker (ArdourCanvas::Item*, GdkEvent*);
	void clear_marker_display ();
	void mouse_add_new_range (Temporal::timepos_t);
	void mouse_add_new_loop (Temporal::timepos_t);
	void mouse_add_new_punch (Temporal::timepos_t);
	bool choose_new_marker_name(std::string &name, bool is_range=false);
	void update_cd_marker_display ();
	void ensure_cd_marker_updated (LocationMarkers* lam, ARDOUR::Location* location);
	void update_cue_marker_display ();
	void ensure_cue_marker_updated (LocationMarkers* lam, ARDOUR::Location* location);

	TimeAxisView*      clicked_axisview;
	RouteTimeAxisView* clicked_routeview;
	/** The last RegionView that was clicked on, or 0 if the last click was not
	 * on a RegionView.  This is set up by the canvas event handlers in
	 * editor_canvas_events.cc
	 */
	RegionView*        clicked_regionview;
	RegionSelection    latest_regionviews;
	uint32_t           clicked_selection;
	ControlPoint*      clicked_control_point;

	void sort_track_selection (TrackViewList&);

	void get_equivalent_regions (RegionView* rv, std::vector<RegionView*> &, PBD::PropertyID) const;
	void get_all_equivalent_regions (RegionView* rv, std::vector<RegionView*> &) const;
	RegionSelection get_equivalent_regions (RegionSelection &, PBD::PropertyID) const;
	RegionView* regionview_from_region (boost::shared_ptr<ARDOUR::Region>) const;
	RouteTimeAxisView* rtav_from_route (boost::shared_ptr<ARDOUR::Route>) const;

	void mapover_tracks_with_unique_playlists (sigc::slot<void,RouteTimeAxisView&,uint32_t> sl, TimeAxisView*, PBD::PropertyID) const;
	void mapover_all_tracks_with_unique_playlists (sigc::slot<void,RouteTimeAxisView&,uint32_t>) const;
	void mapped_get_equivalent_regions (RouteTimeAxisView&, uint32_t, RegionView*, std::vector<RegionView*>*) const;

	void mapover_grouped_routes (sigc::slot<void, RouteUI&> sl, RouteUI*, PBD::PropertyID) const;
	void mapover_armed_routes (sigc::slot<void, RouteUI&> sl) const;
	void mapover_selected_routes (sigc::slot<void, RouteUI&> sl) const;
	void mapover_all_routes (sigc::slot<void, RouteUI&> sl) const;

	void mapped_select_playlist_matching (RouteUI&, boost::weak_ptr<ARDOUR::Playlist> pl);
	void mapped_use_new_playlist (RouteUI&, std::string name, std::string gid, bool copy, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &);
	void mapped_clear_playlist (RouteUI&);

	void new_playlists_for_all_tracks(bool copy);
	void new_playlists_for_grouped_tracks(RouteUI* v, bool copy);
	void new_playlists_for_selected_tracks(bool copy);
	void new_playlists_for_armed_tracks(bool copy);

	void button_selection (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type);
	bool button_release_can_deselect;
	bool _mouse_changed_selection;

	void catch_vanishing_regionview (RegionView*);

	void set_selected_track (TimeAxisView&, Selection::Operation op = Selection::Set, bool no_remove=false);
	void select_all_visible_lanes ();
	void select_all_tracks ();
	bool select_all_internal_edit (Selection::Operation);

	bool set_selected_control_point_from_click (bool press, Selection::Operation op = Selection::Set);
	void set_selected_track_from_click (bool press, Selection::Operation op = Selection::Set, bool no_remove=false);
	void set_selected_track_as_side_effect (Selection::Operation op);
	bool set_selected_regionview_from_click (bool press, Selection::Operation op = Selection::Set);

	bool set_selected_regionview_from_map_event (GdkEventAny*, StreamView*, boost::weak_ptr<ARDOUR::Region>);
	void collect_new_region_view (RegionView*);
	void collect_and_select_new_region_view (RegionView*);

	Gtk::Menu track_context_menu;
	Gtk::Menu track_region_context_menu;
	Gtk::Menu track_selection_context_menu;

	GdkEvent context_click_event;

	void popup_track_context_menu (int, int, ItemType, bool);
	Gtk::Menu* build_track_context_menu ();
	Gtk::Menu* build_track_bus_context_menu ();
	Gtk::Menu* build_track_region_context_menu ();
	Gtk::Menu* build_track_selection_context_menu ();
	void add_dstream_context_items (Gtk::Menu_Helpers::MenuList&);
	void add_bus_context_items (Gtk::Menu_Helpers::MenuList&);
	void add_region_context_items (Gtk::Menu_Helpers::MenuList&, boost::shared_ptr<ARDOUR::Track>);
	void add_selection_context_items (Gtk::Menu_Helpers::MenuList&);
	Gtk::MenuItem* _popup_region_menu_item;

	void popup_control_point_context_menu (ArdourCanvas::Item*, GdkEvent*);
	Gtk::Menu _control_point_context_menu;

	void popup_note_context_menu (ArdourCanvas::Item*, GdkEvent*);
	Gtk::Menu _note_context_menu;

	void initial_display ();
	void add_stripables (ARDOUR::StripableList&);
	void add_routes (ARDOUR::RouteList&);
	void timeaxisview_deleted (TimeAxisView*);
	void add_vcas (ARDOUR::VCAList&);

	Gtk::HBox global_hpacker;
	Gtk::VBox global_vpacker;

	/* Cursor stuff.  Do not use directly, use via CursorContext. */
	friend class CursorContext;
	std::vector<Gdk::Cursor*> _cursor_stack;
	void set_canvas_cursor (Gdk::Cursor*);
	size_t push_canvas_cursor (Gdk::Cursor*);
	void pop_canvas_cursor ();

	Gdk::Cursor* which_track_cursor () const;
	Gdk::Cursor* which_mode_cursor () const;
	Gdk::Cursor* which_trim_cursor (bool left_side) const;
	Gdk::Cursor* which_canvas_cursor (ItemType type) const;

	/** Push the appropriate enter/cursor context on item entry. */
	void choose_canvas_cursor_on_entry (ItemType);

	/** Update all enter cursors based on current settings. */
	void update_all_enter_cursors ();

	ArdourCanvas::GtkCanvas* _track_canvas;
	ArdourCanvas::GtkCanvasViewport* _track_canvas_viewport;

	bool within_track_canvas;

	friend class VerboseCursor;
	VerboseCursor* _verbose_cursor;

	RegionPeakCursor* _region_peak_cursor;

	void parameter_changed (std::string);
	void ui_parameter_changed (std::string);

	Gtk::EventBox            time_bars_event_box;
	Gtk::VBox                time_bars_vbox;

	ArdourCanvas::Container* tempo_group;
	ArdourCanvas::Container* meter_group;
	ArdourCanvas::Container* marker_group;
	ArdourCanvas::Container* range_marker_group;
	ArdourCanvas::Container* transport_marker_group;
	ArdourCanvas::Container* cd_marker_group;
	ArdourCanvas::Container* cue_marker_group;

	/* parent for groups which themselves contain time markers */
	ArdourCanvas::Container* _time_markers_group;

	/* The group containing all other groups that are scrolled vertically
	   and horizontally.
	*/
	ArdourCanvas::ScrollGroup* hv_scroll_group;

	/* The group containing all other groups that are scrolled horizontally ONLY
	*/
	ArdourCanvas::ScrollGroup* h_scroll_group;

	/* Scroll group for cursors, scrolled horizontally, above everything else
	*/
	ArdourCanvas::ScrollGroup* cursor_scroll_group;

	/* The group containing all trackviews. */
	ArdourCanvas::Container* no_scroll_group;

	/* The group containing all trackviews. */
	ArdourCanvas::Container* _trackview_group;

	/* The group holding things (mostly regions) while dragging so they
	 * are on top of everything else
	 */
	ArdourCanvas::Container* _drag_motion_group;

	/* a rect that sits at the bottom of all tracks to act as a drag-no-drop/clickable
	 * target area.
	 */
	ArdourCanvas::Rectangle* _canvas_drop_zone;
	bool canvas_drop_zone_event (GdkEvent* event);

	Glib::RefPtr<Gtk::ToggleAction> ruler_timecode_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_bbt_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_samples_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_minsec_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_tempo_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_meter_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_marker_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_range_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_loop_punch_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_cd_marker_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_cue_marker_action;
	bool                            no_ruler_shown_update;

	Gtk::Widget* ruler_grabbed_widget;

	RulerDialog* ruler_dialog;

	void initialize_rulers ();
	void update_just_timecode ();
	void compute_fixed_ruler_scale (); //calculates the RulerScale of the fixed rulers
	void update_fixed_rulers ();
	void update_tempo_based_rulers ();
	void popup_ruler_menu (Temporal::timepos_t const & where = Temporal::timepos_t (), ItemType type = RegionItem);
	void update_ruler_visibility ();
	void toggle_ruler_visibility ();
	void ruler_toggled (int);
	bool ruler_label_button_release (GdkEventButton*);
	void store_ruler_visibility ();
	void restore_ruler_visibility ();
	void show_rulers_for_grid ();

	enum MinsecRulerScale {
		minsec_show_msecs,
		minsec_show_seconds,
		minsec_show_minutes,
		minsec_show_hours,
		minsec_show_many_hours
	};

	MinsecRulerScale minsec_ruler_scale;

	samplecnt_t minsec_mark_interval;
	gint minsec_mark_modulo;
	gint minsec_nmarks;
	void set_minsec_ruler_scale (samplepos_t, samplepos_t);

	enum TimecodeRulerScale {
		timecode_show_bits,
		timecode_show_samples,
		timecode_show_seconds,
		timecode_show_minutes,
		timecode_show_hours,
		timecode_show_many_hours
	};

	TimecodeRulerScale timecode_ruler_scale;

	gint timecode_mark_modulo;
	gint timecode_nmarks;
	void set_timecode_ruler_scale (samplepos_t, samplepos_t);

	samplecnt_t _samples_ruler_interval;
	void set_samples_ruler_scale (samplepos_t, samplepos_t);

	enum BBTRulerScale {
		bbt_show_many,
		bbt_show_64,
		bbt_show_16,
		bbt_show_4,
		bbt_show_1,
		bbt_show_quarters,
		bbt_show_eighths,
		bbt_show_sixteenths,
		bbt_show_thirtyseconds,
		bbt_show_sixtyfourths,
		bbt_show_onetwentyeighths
	};

	BBTRulerScale bbt_ruler_scale;

	uint32_t bbt_bars;
	gint bbt_nmarks;
	uint32_t bbt_bar_helper_on;
	void compute_bbt_ruler_scale (samplepos_t lower, samplepos_t upper);

	ArdourCanvas::Ruler* timecode_ruler;
	ArdourCanvas::Ruler* bbt_ruler;
	ArdourCanvas::Ruler* samples_ruler;
	ArdourCanvas::Ruler* minsec_ruler;

	static double timebar_height;
	guint32 visible_timebars;
	Gtk::Menu* editor_ruler_menu;

	ArdourCanvas::Rectangle* tempo_bar;
	ArdourCanvas::Rectangle* meter_bar;
	ArdourCanvas::Rectangle* marker_bar;
	ArdourCanvas::Rectangle* range_marker_bar;
	ArdourCanvas::Rectangle* transport_marker_bar;
	ArdourCanvas::Rectangle* cd_marker_bar;
	ArdourCanvas::Rectangle* cue_marker_bar;

	void toggle_cue_behavior ();

	Gtk::Label  minsec_label;
	Gtk::Label  bbt_label;
	Gtk::Label  timecode_label;
	Gtk::Label  samples_label;
	Gtk::Label  tempo_label;
	Gtk::Label  meter_label;
	Gtk::Label  mark_label;
	Gtk::Label  range_mark_label;
	Gtk::Label  transport_mark_label;
	Gtk::Label  cd_mark_label;
	Gtk::Label  cue_mark_label;

	/* videtimline related actions */
	Gtk::Label                videotl_label;
	ArdourCanvas::Container*      videotl_group;
	Glib::RefPtr<Gtk::ToggleAction> ruler_video_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_proc_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_ontop_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_timecode_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_frame_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_osdbg_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_fullscreen_action;
	Glib::RefPtr<Gtk::ToggleAction> xjadeo_letterbox_action;
	Glib::RefPtr<Gtk::Action> xjadeo_zoom_100;
	void set_xjadeo_proc ();
	void toggle_xjadeo_proc (int state=-1);
	void set_close_video_sensitive (bool onoff);
	void set_xjadeo_sensitive (bool onoff);
	void set_xjadeo_viewoption (int);
	void toggle_xjadeo_viewoption (int what, int state=-1);
	void toggle_ruler_video (bool onoff) {ruler_video_action->set_active(onoff);}
	int videotl_bar_height; /* in units of timebar_height; default: 4 */
	int get_videotl_bar_height () const { return videotl_bar_height; }
	void toggle_region_video_lock ();

	EditorCursor* playhead_cursor () const { return _playhead_cursor; }
	EditorCursor* snapped_cursor () const { return _snapped_cursor; }

	samplepos_t playhead_cursor_sample () const;

	Temporal::timepos_t get_region_boundary (Temporal::timepos_t const & pos, int32_t dir, bool with_selection, bool only_onscreen);

	void    cursor_to_region_boundary (bool with_selection, int32_t dir);
	void    cursor_to_next_region_boundary (bool with_selection);
	void    cursor_to_previous_region_boundary (bool with_selection);
	void    cursor_to_next_region_point (EditorCursor*, ARDOUR::RegionPoint);
	void    cursor_to_previous_region_point (EditorCursor*, ARDOUR::RegionPoint);
	void    cursor_to_region_point (EditorCursor*, ARDOUR::RegionPoint, int32_t dir);
	void    cursor_to_selection_start (EditorCursor*);
	void    cursor_to_selection_end   (EditorCursor*);

	void    selected_marker_to_region_boundary (bool with_selection, int32_t dir);
	void    selected_marker_to_next_region_boundary (bool with_selection);
	void    selected_marker_to_previous_region_boundary (bool with_selection);
	void    selected_marker_to_next_region_point (ARDOUR::RegionPoint);
	void    selected_marker_to_previous_region_point (ARDOUR::RegionPoint);
	void    selected_marker_to_region_point (ARDOUR::RegionPoint, int32_t dir);
	void    selected_marker_to_selection_start ();
	void    selected_marker_to_selection_end   ();

	void    select_all_selectables_using_cursor (EditorCursor*, bool);
	void    select_all_selectables_using_edit (bool, bool);
	void    select_all_selectables_between (bool within);
	void    select_range_between ();

	boost::shared_ptr<ARDOUR::Region> find_next_region (Temporal::timepos_t const &, ARDOUR::RegionPoint, int32_t dir, TrackViewList&, TimeAxisView** = 0);
	Temporal::timepos_t find_next_region_boundary (Temporal::timepos_t const &, int32_t dir, const TrackViewList&);

	std::vector<Temporal::timepos_t> region_boundary_cache;
	void mark_region_boundary_cache_dirty () { _region_boundary_cache_dirty = true; }
	void build_region_boundary_cache ();
	bool	_region_boundary_cache_dirty;

	Gtk::HBox           toplevel_hpacker;

	Gtk::HBox           bottom_hbox;

	Gtk::Table          edit_packer;

	/** the adjustment that controls the overall editor vertical scroll position */
	Gtk::Adjustment     vertical_adjustment;
	Gtk::Adjustment     horizontal_adjustment;

	Gtk::Adjustment     unused_adjustment; // yes, really; Gtk::Layout constructor requires refs
	Gtk::Layout         controls_layout;
	bool control_layout_scroll (GdkEventScroll* ev);
	void reset_controls_layout_width ();
	void reset_controls_layout_height (int32_t height);

	enum Direction {
		LEFT,
		RIGHT,
		UP,
		DOWN
	};

	bool scroll_press (Direction);
	void scroll_release ();
	sigc::connection _scroll_connection;
	int _scroll_callbacks;

	double _visible_canvas_width;
	double _visible_canvas_height; ///< height of the visible area of the track canvas
	double _full_canvas_height;    ///< full height of the canvas

	bool track_canvas_map_handler (GdkEventAny*);

	bool edit_controls_button_event (GdkEventButton*);
	Gtk::Menu* edit_controls_left_menu;
	Gtk::Menu* edit_controls_right_menu;

	Gtk::VBox           track_canvas_vbox;
	Gtk::VBox           edit_controls_vbox;
	Gtk::HBox           edit_controls_hbox;

	void control_vertical_zoom_in_all ();
	void control_vertical_zoom_out_all ();
	void control_vertical_zoom_in_selected ();
	void control_vertical_zoom_out_selected ();
	void control_step_tracks_up ();
	void control_step_tracks_down ();
	void control_view (uint32_t);
	void control_scroll (float);
	void access_action (const std::string&, const std::string&);
	void set_toggleaction (const std::string&, const std::string&, bool);
	bool deferred_control_scroll (samplepos_t);
	sigc::connection control_scroll_connection;

	void tie_vertical_scrolling ();
	void set_horizontal_position (double);
	double horizontal_position () const;

	struct VisualChange {
		enum Type {
			TimeOrigin = 0x1,
			ZoomLevel = 0x2,
			YOrigin = 0x4,
			VideoTimeline = 0x8
		};

		Type        pending;
		samplepos_t time_origin;
		samplecnt_t samples_per_pixel;
		double      y_origin;

		int idle_handler_id;
		/** true if we are currently in the idle handler */
		bool being_handled;

		VisualChange() : pending ((VisualChange::Type) 0), time_origin (0), samples_per_pixel (0), idle_handler_id (-1), being_handled (false) {}
		void add (Type t) {
			pending = Type (pending | t);
		}
	};

	VisualChange pending_visual_change;
	bool visual_change_queued;

	void pre_render ();

	static int _idle_visual_changer (void* arg);
	int idle_visual_changer ();
	void visual_changer (const VisualChange&);
	void ensure_visual_change_idle_handler ();

	/* track views */
	TrackViewList track_views;

	std::pair<TimeAxisView*, double> trackview_by_y_position (double, bool trackview_relative_offset = true) const;

	AxisView* axis_view_by_stripable (boost::shared_ptr<ARDOUR::Stripable>) const;
	AxisView* axis_view_by_control (boost::shared_ptr<ARDOUR::AutomationControl>) const;

	TimeAxisView* time_axis_view_from_stripable (boost::shared_ptr<ARDOUR::Stripable> s) const {
		return dynamic_cast<TimeAxisView*> (axis_view_by_stripable (s));
	}

	TrackViewList get_tracks_for_range_action () const;

	Gtk::VBox list_vpacker;

	void queue_redisplay_track_views ();
	void process_redisplay_track_views ();
	void redisplay_track_views_now ();
	bool redisplay_track_views (); // do not call this directly, use above wrappers

	bool             _tvl_no_redisplay;
	bool             _tvl_redisplay_on_resume;
	sigc::connection _tvl_redisplay_connection;

	sigc::connection super_rapid_screen_update_connection;
	void center_screen_internal (samplepos_t, float);

	void super_rapid_screen_update ();

	int64_t _last_update_time;
	double _err_screen_engine;

	void session_going_away ();

	samplepos_t cut_buffer_start;
	samplecnt_t cut_buffer_length;

	boost::shared_ptr<CursorContext> _press_cursor_ctx;  ///< Button press cursor context

	boost::weak_ptr<ARDOUR::Trimmable> _trimmable;
	boost::weak_ptr<ARDOUR::Movable> _movable;

	bool typed_event (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler_1 (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler_2 (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_double_click_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_dispatch (GdkEventButton*);
	bool button_release_dispatch (GdkEventButton*);
	bool motion_handler (ArdourCanvas::Item*, GdkEvent*, bool from_autoscroll = false);
	bool enter_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool leave_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool key_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool key_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);

	Gtkmm2ext::Bindings* button_bindings;
	XMLNode* button_settings () const;

	/* KEYMAP HANDLING */

	void register_actions ();
	void register_region_actions ();
	void register_midi_actions (Gtkmm2ext::Bindings*);

	void load_bindings ();

	/* CUT/COPY/PASTE */

	Temporal::timepos_t last_paste_pos;
	unsigned    paste_count;

	void cut_copy (Editing::CutCopyOp);
	bool can_cut_copy () const;
	void cut_copy_points (Editing::CutCopyOp, Temporal::timepos_t const & earliest);
	void cut_copy_regions (Editing::CutCopyOp, RegionSelection&);
	void cut_copy_ranges (Editing::CutCopyOp);
	void cut_copy_midi (Editing::CutCopyOp);

	void mouse_paste ();
	void paste_internal (Temporal::timepos_t const & position, float times);

	/* EDITING OPERATIONS */

	void reset_point_selection ();
	void toggle_region_lock ();
	void toggle_opaque_region ();
	void toggle_record_enable ();
	void toggle_solo ();
	void toggle_solo_isolate ();
	void toggle_mute ();
	void toggle_region_lock_style ();

	void play_solo_selection (bool restart);

	enum LayerOperation {
		Raise,
		RaiseToTop,
		Lower,
		LowerToBottom
	};

	void do_layer_operation (LayerOperation);
	void raise_region ();
	void raise_region_to_top ();
	void change_region_layering_order (bool from_context_menu);
	void lower_region ();
	void lower_region_to_bottom ();
	void split_region_at_transients ();
	void crop_region_to_selection ();
	void crop_region_to (Temporal::timepos_t const & start, Temporal::timepos_t const & end);
	void set_sync_point (Temporal::timepos_t const &, const RegionSelection&);
	void set_region_sync_position ();
	void remove_region_sync();
	void align_regions (ARDOUR::RegionPoint);
	void align_regions_relative (ARDOUR::RegionPoint point);
	void align_region (boost::shared_ptr<ARDOUR::Region>, ARDOUR::RegionPoint point, Temporal::timepos_t const & position);
	void align_region_internal (boost::shared_ptr<ARDOUR::Region>, ARDOUR::RegionPoint point, Temporal::timepos_t const & position);
	void recover_regions (ARDOUR::RegionList);
	void remove_selected_regions ();
	void remove_regions (const RegionSelection&, bool can_ripple, bool as_part_of_other_command);
	void remove_clicked_region ();
	void show_region_properties ();
	void show_midi_list_editor ();
	void rename_region ();
	void duplicate_some_regions (RegionSelection&, float times);
	void duplicate_selection (float times);
	void region_fill_selection ();
	void combine_regions ();
	void uncombine_regions ();

	void region_fill_track ();
	void audition_playlist_region_standalone (boost::shared_ptr<ARDOUR::Region>);
	void split_multichannel_region();
	void reverse_region ();
	void strip_region_silence ();
	void normalize_region ();
	void reset_region_scale_amplitude ();
	void adjust_region_gain (bool up);
	void reset_region_gain ();
	void quantize_region ();
	void quantize_regions (const RegionSelection& rs);
	void legatize_region (bool shrink_only);
	void legatize_regions (const RegionSelection& rs, bool shrink_only);
	void deinterlace_midi_regions (const RegionSelection& rs);
	void deinterlace_selected_midi_regions ();
	void transform_region ();
	void transform_regions (const RegionSelection& rs);
	void transpose_region ();
	void transpose_regions (const RegionSelection& rs);
	void insert_patch_change (bool from_context);
	void fork_region ();

	void do_insert_time ();
	void insert_time (Temporal::timepos_t const &, Temporal::timecnt_t const &, Editing::InsertTimeOption, bool, bool, bool, bool, bool, bool);

	void do_remove_time ();
	void remove_time (Temporal::timepos_t const & pos, Temporal::timecnt_t const & distance, Editing::InsertTimeOption opt, bool ignore_music_glue, bool markers_too,
	                  bool glued_markers_too, bool locked_markers_too, bool tempo_too);

	void tab_to_transient (bool forward);

	void set_tempo_from_region ();
	void use_range_as_bar ();

	void define_one_bar (Temporal::timepos_t const & start, Temporal::timepos_t const & end);

	void audition_region_from_region_list ();

	void naturalize_region ();

	void split_region ();

	void delete_ ();
	void cut ();
	void copy ();
	void paste (float times, bool from_context_menu);

	void place_transient ();
	void remove_transient (ArdourCanvas::Item* item);
	void snap_regions_to_grid ();
	void close_region_gaps ();

	void keyboard_paste ();

	void region_from_selection ();
	void create_region_from_selection (std::vector<boost::shared_ptr<ARDOUR::Region> >&);

	void play_from_start ();
	void play_from_edit_point ();
	void play_from_edit_point_and_return ();
	void play_selected_region ();
	void play_edit_range ();
	void play_location (ARDOUR::Location&);
	void loop_location (ARDOUR::Location&);

	void calc_extra_zoom_edges(samplepos_t &start, samplepos_t &end);
	void temporal_zoom_selection (Editing::ZoomAxis);
	void temporal_zoom_session ();
	void temporal_zoom_extents ();
	void temporal_zoom (samplecnt_t samples_per_pixel);
	void temporal_zoom_by_sample (samplepos_t start, samplepos_t end);
	void temporal_zoom_to_sample (bool coarser, samplepos_t sample);

	void insert_source_list_selection (float times);

	/* import & embed */

	void add_external_audio_action (Editing::ImportMode);

	int  check_whether_and_how_to_import(std::string, bool all_or_nothing = true);
	bool check_multichannel_status (const std::vector<std::string>& paths);

	SoundFileOmega* sfbrowser;

	void bring_in_external_audio (Editing::ImportMode mode,  samplepos_t& pos);

	bool  idle_drop_paths  (std::vector<std::string> paths, Temporal::timepos_t sample, double ypos, bool copy);
	void  drop_paths_part_two  (const std::vector<std::string>& paths, Temporal::timepos_t const & sample, double ypos, bool copy);

	int import_sndfiles (std::vector<std::string>              paths,
	                     Editing::ImportDisposition            disposition,
	                     Editing::ImportMode                   mode,
	                     ARDOUR::SrcQuality                    quality,
	                     Temporal::timepos_t&                  pos,
	                     int                                   target_regions,
	                     int                                   target_tracks,
	                     boost::shared_ptr<ARDOUR::Track>&     track,
	                     std::string const&                    pgroup_id,
	                     bool                                  replace,
	                     boost::shared_ptr<ARDOUR::PluginInfo> instrument = boost::shared_ptr<ARDOUR::PluginInfo>());

	int embed_sndfiles (std::vector<std::string>              paths,
	                    bool                                  multiple_files,
	                    bool&                                 check_sample_rate,
	                    Editing::ImportDisposition            disposition,
	                    Editing::ImportMode                   mode,
	                    Temporal::timepos_t&                  pos,
	                    int                                   target_regions,
	                    int                                   target_tracks,
	                    boost::shared_ptr<ARDOUR::Track>&     track,
	                    std::string const&                    pgroup_id,
	                    boost::shared_ptr<ARDOUR::PluginInfo> instrument = boost::shared_ptr<ARDOUR::PluginInfo>());

	int add_sources (std::vector<std::string>              paths,
	                 ARDOUR::SourceList&                   sources,
	                 Temporal::timepos_t&                  pos,
	                 Editing::ImportDisposition            disposition,
	                 Editing::ImportMode                   mode,
	                 int                                   target_regions,
	                 int                                   target_tracks,
	                 boost::shared_ptr<ARDOUR::Track>&     track,
	                 std::string const&                    pgroup_id,
	                 bool                                  add_channel_suffix,
	                 boost::shared_ptr<ARDOUR::PluginInfo> instrument = boost::shared_ptr<ARDOUR::PluginInfo>());

	int finish_bringing_in_material (boost::shared_ptr<ARDOUR::Region>     region,
	                                 uint32_t                              in_chans,
	                                 uint32_t                              out_chans,
	                                 Temporal::timepos_t&                  pos,
	                                 Editing::ImportMode                   mode,
	                                 boost::shared_ptr<ARDOUR::Track>&     existing_track,
	                                 std::string const&                    new_track_name,
	                                 std::string const&                    pgroup_id,
	                                 boost::shared_ptr<ARDOUR::PluginInfo> instrument);

	boost::shared_ptr<ARDOUR::AudioTrack> get_nth_selected_audio_track (int nth) const;
	boost::shared_ptr<ARDOUR::MidiTrack> get_nth_selected_midi_track (int nth) const;

	void toggle_midi_input_active (bool flip_others);

	ARDOUR::InterThreadInfo* current_interthread_info;

	AnalysisWindow* analysis_window;

	/* import & embed */
	void external_audio_dialog ();
	void session_import_dialog ();

	/* PT import specific */
	void external_pt_dialog ();
	ARDOUR::ImportStatus import_pt_status;
	static void*_import_pt_thread (void*);
	void* import_pt_thread ();
	PTFFormat import_ptf;

	/* import specific info */

	struct EditorImportStatus : public ARDOUR::ImportStatus {
		void clear () {
			ARDOUR::ImportStatus::clear ();
			track.reset ();
		}

		Editing::ImportMode mode;
		Temporal::timepos_t pos;
		int target_tracks;
		int target_regions;
		boost::shared_ptr<ARDOUR::Track> track;
		bool replace;
	};

	EditorImportStatus import_status;
	static void*_import_thread (void*);
	void* import_thread ();
	void finish_import ();

	/* to support this ... */

	void import_audio (bool as_tracks);
	void do_import (std::vector<std::string> paths, bool split, bool as_tracks);
	void import_smf_tempo_map (Evoral::SMF const &, Temporal::timepos_t const & pos);
	void import_smf_markers (Evoral::SMF &, Temporal::timepos_t const & pos);
	void move_to_start ();
	void move_to_end ();
	void center_playhead ();
	void center_edit_point ();
	void playhead_forward_to_grid ();
	void playhead_backward_to_grid ();
	void scroll_playhead (bool forward);
	void scroll_backward (float pages=0.8f);
	void scroll_forward (float pages=0.8f);
	void scroll_tracks_down ();
	void scroll_tracks_up ();
	void move_selected_tracks (bool);
	void set_mark ();
	void clear_markers ();
	void clear_xrun_markers ();
	void clear_ranges ();
	void clear_locations ();
	void unhide_markers ();
	void unhide_ranges ();
	void cursor_align (bool playhead_to_edit);
	void toggle_skip_playback ();

	void remove_last_capture ();

	void tag_last_capture ();
	void tag_selected_region ();
	void tag_regions (ARDOUR::RegionList);

	void select_all_selectables_using_time_selection ();
	void select_all_selectables_using_loop();
	void select_all_selectables_using_punch();
	void set_selection_from_range (ARDOUR::Location&);
	void set_selection_from_punch ();
	void set_selection_from_loop ();
	void set_selection_from_region ();

	void add_location_mark (Temporal::timepos_t const & where);
	void add_location_from_region ();
	void add_locations_from_region ();
	void add_location_from_selection ();
	void set_loop_from_selection (bool play);
	void set_punch_from_selection ();
	void set_punch_from_region ();
	void set_auto_punch_range();

	void set_session_start_from_playhead ();
	void set_session_end_from_playhead ();
	void set_session_extents_from_selection ();

	void set_loop_from_region (bool play);

	void set_loop_range (Temporal::timepos_t const & start, Temporal::timepos_t const & end, std::string cmd);
	void set_punch_range (Temporal::timepos_t const & start, Temporal::timepos_t const & end, std::string cmd);

	void toggle_location_at_playhead_cursor ();
	void add_location_from_playhead_cursor ();
	bool do_remove_location_at_playhead_cursor ();
	void remove_location_at_playhead_cursor ();
	bool select_new_marker;

	void toggle_all_existing_automation ();

	void toggle_layer_display ();
	void layer_display_stacked ();
	void layer_display_overlaid ();

	void launch_playlist_selector ();

	void reverse_selection ();
	void edit_envelope ();

	double last_scrub_x;
	int scrubbing_direction;
	int scrub_reversals;
	int scrub_reverse_distance;
	void scrub (samplepos_t, double);

	void set_punch_start_from_edit_point ();
	void set_punch_end_from_edit_point ();
	void set_loop_start_from_edit_point ();
	void set_loop_end_from_edit_point ();

	void keyboard_selection_begin (Editing::EditIgnoreOption = Editing::EDIT_IGNORE_NONE);
	void keyboard_selection_finish (bool add, Editing::EditIgnoreOption = Editing::EDIT_IGNORE_NONE);
	bool have_pending_keyboard_selection;
	samplepos_t pending_keyboard_selection_start;

	void move_range_selection_start_or_end_to_region_boundary (bool, bool);

	Editing::GridType _grid_type;
	Editing::SnapMode _snap_mode;

	Editing::GridType _draw_length;
	int _draw_velocity;
	int _draw_channel;

	bool ignore_gui_changes;

	DragManager* _drags;

	void escape ();
	void lock ();
	void unlock ();
	Gtk::Dialog* lock_dialog;

	struct timeval last_event_time;
	bool generic_event_handler (GdkEvent*);
	bool lock_timeout_callback ();
	void start_lock_event_timing ();

	Gtk::Menu fade_context_menu;

	Gtk::Menu xfade_in_context_menu;
	Gtk::Menu xfade_out_context_menu;
	void popup_xfade_in_context_menu (int, int, ArdourCanvas::Item*, ItemType);
	void popup_xfade_out_context_menu (int, int, ArdourCanvas::Item*, ItemType);
	void fill_xfade_menu (Gtk::Menu_Helpers::MenuList& items, bool start);

	void set_fade_in_shape (ARDOUR::FadeShape);
	void set_fade_out_shape (ARDOUR::FadeShape);

	void set_fade_length (bool in);
	void set_fade_in_active (bool);
	void set_fade_out_active (bool);

	void fade_range ();

	std::set<boost::shared_ptr<ARDOUR::Playlist> > motion_frozen_playlists;

	bool _dragging_playhead;

	void marker_drag_motion_callback (GdkEvent*);
	void marker_drag_finished_callback (GdkEvent*);

	gint mouse_rename_region (ArdourCanvas::Item*, GdkEvent*);

	void add_region_drag (ArdourCanvas::Item*, GdkEvent*, RegionView*, bool copy);
	void start_create_region_grab (ArdourCanvas::Item*, GdkEvent*);
	void add_region_brush_drag (ArdourCanvas::Item*, GdkEvent*, RegionView*);
	void start_selection_grab (ArdourCanvas::Item*, GdkEvent*);

	void region_view_item_click (AudioRegionView&, GdkEventButton*);

	bool can_remove_control_point (ArdourCanvas::Item*);
	void remove_control_point (ArdourCanvas::Item*);

	/* Canvas event handlers */

	bool canvas_scroll_event (GdkEventScroll* event, bool from_canvas);
	bool canvas_control_point_event (GdkEvent* event,ArdourCanvas::Item*, ControlPoint*);
	bool canvas_line_event (GdkEvent* event,ArdourCanvas::Item*, AutomationLine*);
	bool canvas_selection_rect_event (GdkEvent* event,ArdourCanvas::Item*, SelectionRect*);
	bool canvas_selection_start_trim_event (GdkEvent* event,ArdourCanvas::Item*, SelectionRect*);
	bool canvas_selection_end_trim_event (GdkEvent* event,ArdourCanvas::Item*, SelectionRect*);
	bool canvas_start_xfade_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_end_xfade_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_fade_in_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_fade_in_handle_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*, bool trim = false);
	bool canvas_fade_out_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_fade_out_handle_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*, bool trim = false);
	bool canvas_region_view_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_wave_view_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_frame_handle_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_region_view_name_highlight_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_region_view_name_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_feature_line_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*);
	bool canvas_stream_view_event (GdkEvent* event,ArdourCanvas::Item*, RouteTimeAxisView*);
	bool canvas_marker_event (GdkEvent* event,ArdourCanvas::Item*, ArdourMarker*);
	bool canvas_tempo_marker_event (GdkEvent* event,ArdourCanvas::Item*, TempoMarker*);
	bool canvas_tempo_curve_event (GdkEvent* event,ArdourCanvas::Item*, TempoCurve*);
	bool canvas_meter_marker_event (GdkEvent* event,ArdourCanvas::Item*, MeterMarker*);
	bool canvas_bbt_marker_event (GdkEvent* event,ArdourCanvas::Item*, BBTMarker*);
	bool canvas_automation_track_event(GdkEvent* event, ArdourCanvas::Item*, AutomationTimeAxisView*);
	bool canvas_note_event (GdkEvent* event, ArdourCanvas::Item*);

	bool canvas_ruler_event (GdkEvent* event, ArdourCanvas::Item*, ItemType);
	bool canvas_tempo_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_meter_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_range_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_transport_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_cd_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_cue_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);

	bool canvas_videotl_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	void update_video_timeline (bool flush = false);
	void set_video_timeline_height (const int);
	bool is_video_timeline_locked ();
	void toggle_video_timeline_locked ();
	void set_video_timeline_locked (const bool);
	void queue_visual_videotimeline_update ();
	void embed_audio_from_video (std::string, samplepos_t n = 0, bool lock_position_to_video = true);

	bool track_selection_change_without_scroll () const {
		return _track_selection_change_without_scroll;
	}

	PBD::Signal0<void> EditorFreeze;
	PBD::Signal0<void> EditorThaw;

	void begin_tempo_map_edit ();
	void abort_tempo_map_edit ();
	void commit_tempo_map_edit ();
	void mid_tempo_change ();

private:
	friend class DragManager;
	friend class EditorRouteGroups;
	friend class EditorRegions;
	friend class EditorSources;

	/* non-public event handlers */

	bool canvas_playhead_cursor_event (GdkEvent* event, ArdourCanvas::Item*);
	bool track_canvas_scroll (GdkEventScroll* event);

	bool track_canvas_button_press_event (GdkEventButton* event);
	bool track_canvas_button_release_event (GdkEventButton* event);
	bool track_canvas_motion_notify_event (GdkEventMotion* event);

	Gtk::Allocation _canvas_viewport_allocation;
	void track_canvas_viewport_allocate (Gtk::Allocation alloc);
	void track_canvas_viewport_size_allocated ();
	bool track_canvas_drag_motion (Glib::RefPtr<Gdk::DragContext> const &, int, int, guint);
	bool track_canvas_key_press (GdkEventKey*);
	bool track_canvas_key_release (GdkEventKey*);

	void set_playhead_cursor ();

	void toggle_region_mute ();

	void initialize_canvas ();

	/* playlist internal ops */

	bool stamp_new_playlist (std::string title, std::string &name, std::string &pgroup, bool copy);

	/* display control */

	/// true if the editor should follow the playhead, otherwise false
	bool _follow_playhead;
	/// true if we scroll the tracks rather than the playhead
	bool _stationary_playhead;
	/// true if we are in fullscreen mode
	bool _maximised;

	std::vector<ArdourCanvas::Ruler::Mark> grid_marks;
	GridLines* grid_lines;

	ArdourCanvas::Container* global_rect_group;
	ArdourCanvas::Container* time_line_group;

	void hide_grid_lines ();
	void maybe_draw_grid_lines ();

	void new_tempo_section ();

	void remove_tempo_marker (ArdourCanvas::Item*);
	void remove_meter_marker (ArdourCanvas::Item*);
	gint real_remove_tempo_marker (Temporal::TempoPoint const *);
	gint real_remove_meter_marker (Temporal::MeterPoint const *);

	void edit_tempo_marker (TempoMarker&);
	void edit_meter_marker (MeterMarker&);
	void edit_control_point (ArdourCanvas::Item*);
	void edit_notes (MidiRegionView*);
	void edit_region (RegionView*);

	void edit_current_meter ();
	void edit_current_tempo ();

	void marker_menu_edit ();
	void marker_menu_remove ();
	void marker_menu_rename ();
	void rename_marker (ArdourMarker* marker);
	void toggle_tempo_clamped ();
	void toggle_tempo_type ();
	void ramp_to_next_tempo ();
	void toggle_marker_menu_lock ();
	void toggle_marker_menu_glue ();
	void marker_menu_hide ();
	void marker_menu_set_origin ();
	void marker_menu_loop_range ();
	void marker_menu_select_all_selectables_using_range ();
	void marker_menu_select_using_range ();
	void marker_menu_separate_regions_using_location ();
	void marker_menu_play_from ();
	void marker_menu_play_range ();
	void marker_menu_set_playhead ();
	void marker_menu_set_from_playhead ();
	void marker_menu_set_from_selection (bool force_regions);
	void marker_menu_range_to_next ();
	void marker_menu_change_cue (int cue);
	void marker_menu_zoom_to_range ();
	void new_transport_marker_menu_set_loop ();
	void new_transport_marker_menu_set_punch ();
	void update_loop_range_view ();
	void update_punch_range_view ();
	void new_transport_marker_menu_popdown ();
	void marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void tempo_or_meter_marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void new_transport_marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void build_range_marker_menu (ARDOUR::Location*, bool, bool);
	void build_marker_menu (ARDOUR::Location*);
	void build_tempo_marker_menu (TempoMarker*, bool);
	void build_meter_marker_menu (MeterMarker*, bool);
	void build_new_transport_marker_menu ();

	void dynamic_cast_marker_object (void*, MeterMarker**, TempoMarker**) const;

	Gtk::Menu* tempo_marker_menu;
	Gtk::Menu* meter_marker_menu;
	Gtk::Menu* marker_menu;
	Gtk::Menu* range_marker_menu;
	Gtk::Menu* new_transport_marker_menu;
	ArdourCanvas::Item* marker_menu_item;

	typedef std::list<MetricMarker*> Marks;
	Marks tempo_marks;
	Marks meter_marks;
	Marks bbt_marks;

	void remove_metric_marks ();
	void draw_metric_marks (Temporal::TempoMap::Metrics const & metrics);
	void draw_tempo_marks ();
	void draw_meter_marks ();
	void draw_bbt_marks ();

	void compute_current_bbt_points (Temporal::TempoMapPoints& grid, samplepos_t left, samplepos_t right);

	void reassociate_metric_markers (Temporal::TempoMap::SharedPtr const &);
	void reassociate_metric_marker (Temporal::TempoMap::SharedPtr const & tmap, Temporal::TempoMap::Metrics & metric, MetricMarker& marker);
	void make_bbt_marker (Temporal::MusicTimePoint const *);
	void make_meter_marker (Temporal::MeterPoint const *);
	void make_tempo_marker (Temporal::TempoPoint const * ts, double& min_tempo, double& max_tempo, Temporal::TempoPoint const *& prev_ts, uint32_t tc_color, samplecnt_t sr);
	void update_tempo_curves (double min_tempo, double max_tempo, samplecnt_t sr);

	void tempo_map_changed ();

	void redisplay_grid (bool immediate_redraw);

	/* toolbar */

	Gtk::ToggleButton editor_mixer_button;
	Gtk::ToggleButton editor_list_button;
	void editor_mixer_button_toggled ();
	void editor_list_button_toggled ();

	ArdourWidgets::ArdourButton   zoom_in_button;
	ArdourWidgets::ArdourButton   zoom_out_button;
	ArdourWidgets::ArdourButton   zoom_out_full_button;

	ArdourWidgets::ArdourButton   tav_expand_button;
	ArdourWidgets::ArdourButton   tav_shrink_button;
	ArdourWidgets::ArdourDropdown visible_tracks_selector;
	ArdourWidgets::ArdourDropdown zoom_preset_selector;

	int32_t                   _visible_track_count;
	void build_track_count_menu ();
	void set_visible_track_count (int32_t);

	void set_zoom_preset(int64_t);

	Gtk::VBox                toolbar_clock_vbox;
	Gtk::VBox                toolbar_selection_clock_vbox;
	Gtk::Table               toolbar_selection_clock_table;
	Gtk::Label               toolbar_selection_cursor_label;

	ArdourWidgets::ArdourButton mouse_select_button;
	ArdourWidgets::ArdourButton mouse_draw_button;
	ArdourWidgets::ArdourButton mouse_move_button;
	ArdourWidgets::ArdourButton mouse_timefx_button;
	ArdourWidgets::ArdourButton mouse_content_button;
	ArdourWidgets::ArdourButton mouse_audition_button;
	ArdourWidgets::ArdourButton mouse_cut_button;

	ArdourWidgets::ArdourButton smart_mode_button;
	Glib::RefPtr<Gtk::ToggleAction> smart_mode_action;

	void                     mouse_mode_toggled (Editing::MouseMode m);
	void			 mouse_mode_object_range_toggled ();
	bool                     ignore_mouse_mode_toggle;

	bool                     mouse_select_button_release (GdkEventButton*);

	Glib::RefPtr<Gtk::Action> get_mouse_mode_action (Editing::MouseMode m) const;

	Gtk::VBox                automation_box;
	Gtk::Button              automation_mode_button;

	//edit mode menu stuff
	ArdourWidgets::ArdourDropdown	edit_mode_selector;
	void edit_mode_selection_done (ARDOUR::EditMode);
	void build_edit_mode_menu ();
	Gtk::VBox edit_mode_box;

	void set_edit_mode (ARDOUR::EditMode);
	void cycle_edit_mode ();

	ArdourWidgets::ArdourDropdown grid_type_selector;
	void build_grid_type_menu ();

	ArdourWidgets::ArdourDropdown draw_length_selector;
	ArdourWidgets::ArdourDropdown draw_velocity_selector;
	ArdourWidgets::ArdourDropdown draw_channel_selector;

	ArdourWidgets::ArdourButton snap_mode_button;
	bool snap_mode_button_clicked (GdkEventButton*);

	Gtk::HBox snap_box;
	Gtk::HBox draw_box;

	Gtk::HBox ebox_hpacker;
	Gtk::VBox ebox_vpacker;

	Gtk::HBox _box;

	std::vector<std::string> grid_type_strings;
	std::vector<std::string> snap_mode_strings;

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

	Glib::RefPtr<Gtk::RadioAction> grid_type_action (Editing::GridType);
	Glib::RefPtr<Gtk::RadioAction> snap_mode_action (Editing::SnapMode);

	Glib::RefPtr<Gtk::RadioAction> draw_length_action (Editing::GridType);
	Glib::RefPtr<Gtk::RadioAction> draw_velocity_action (int);
	Glib::RefPtr<Gtk::RadioAction> draw_channel_action (int);

	//zoom focus meu stuff
	ArdourWidgets::ArdourDropdown	zoom_focus_selector;
	void zoom_focus_selection_done (Editing::ZoomFocus);
	void build_zoom_focus_menu ();
	std::vector<std::string> zoom_focus_strings;

	void zoom_focus_chosen (Editing::ZoomFocus);

	Glib::RefPtr<Gtk::RadioAction> zoom_focus_action (Editing::ZoomFocus);

	Gtk::HBox _track_box;

	Gtk::HBox _zoom_box;
	void zoom_adjustment_changed();

	void setup_toolbar ();

	void setup_tooltips ();

	Gtk::HBox toolbar_hbox;

	void setup_midi_toolbar ();

	/* selection process */

	Selection* selection;
	Selection* cut_buffer;
	SelectionMemento* _selection_memento;

	void time_selection_changed ();
	void track_selection_changed ();
	void update_time_selection_display ();
	void presentation_info_changed (PBD::PropertyChange const &);
	void handle_gui_changes (std::string const&, void*);
	void region_selection_changed ();
	void catch_up_on_midi_selection ();
	sigc::connection editor_regions_selection_changed_connection;
	void sensitize_all_region_actions (bool);
	void sensitize_the_right_region_actions (bool because_canvas_crossing);
	bool _all_region_actions_sensitized;
	/** Flag to block region action handlers from doing what they normally do;
	 *  I tried Gtk::Action::block_activate() but this doesn't work (ie it doesn't
	 *  block) when setting a ToggleAction's active state.
	 */
	bool _ignore_region_action;
	bool _last_region_menu_was_main;
	void point_selection_changed ();
	void marker_selection_changed ();

	bool _track_selection_change_without_scroll;
	bool _editor_track_selection_change_without_scroll;

	void cancel_selection ();
	void cancel_time_selection ();

	bool get_smart_mode() const;

	bool audio_region_selection_covers (samplepos_t where);

	/* playhead and edit cursor */

	EditorCursor* _playhead_cursor;
	EditorCursor* _snapped_cursor;

	/* transport range select process */

	ArdourCanvas::Rectangle* cd_marker_bar_drag_rect;
	ArdourCanvas::Rectangle* cue_marker_bar_drag_rect;
	ArdourCanvas::Rectangle* range_bar_drag_rect;
	ArdourCanvas::Rectangle* transport_bar_drag_rect;
	ArdourCanvas::Rectangle* transport_bar_range_rect;
	ArdourCanvas::Rectangle* transport_bar_preroll_rect;
	ArdourCanvas::Rectangle* transport_bar_postroll_rect;
	ArdourCanvas::Rectangle* transport_loop_range_rect;
	ArdourCanvas::Rectangle* transport_punch_range_rect;
	ArdourCanvas::Line*      transport_punchin_line;
	ArdourCanvas::Line*      transport_punchout_line;
	ArdourCanvas::Rectangle* transport_preroll_rect;
	ArdourCanvas::Rectangle* transport_postroll_rect;

	ARDOUR::Location* transport_loop_location();
	ARDOUR::Location* transport_punch_location();

	ARDOUR::Location* temp_location;

	/* object rubberband select process */

	void select_all_within (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, TrackViewList const &, Selection::Operation, bool);

	ArdourCanvas::Rectangle* rubberband_rect;

	EditorRouteGroups* _route_groups;
	EditorRoutes*      _routes;
	EditorRegions*     _regions;
	EditorSources*     _sources;
	EditorSnapshots*   _snapshots;
	EditorLocations*   _locations;

	/* diskstream/route display management */
	Glib::RefPtr<Gdk::Pixbuf> rec_enabled_icon;
	Glib::RefPtr<Gdk::Pixbuf> rec_disabled_icon;

	Glib::RefPtr<Gtk::TreeSelection> route_display_selection;

	/* autoscrolling */

	sigc::connection autoscroll_connection;
	bool autoscroll_horizontal_allowed;
	bool autoscroll_vertical_allowed;
	uint32_t autoscroll_cnt;
	Gtk::Widget* autoscroll_widget;
	ArdourCanvas::Rect autoscroll_boundary;

	bool autoscroll_canvas ();
	void start_canvas_autoscroll (bool allow_horiz, bool allow_vert, const ArdourCanvas::Rect& boundary);
	void stop_canvas_autoscroll ();

	/* trimming */
	void point_trim (GdkEvent*, Temporal::timepos_t const &);

	void trim_region_front();
	void trim_region_back();
	void trim_region (bool front);

	void trim_region_to_loop ();
	void trim_region_to_punch ();
	void trim_region_to_location (const ARDOUR::Location&, const char* cmd);

	void trim_to_region(bool forward);
	void trim_region_to_previous_region_end();
	void trim_region_to_next_region_start();

	bool show_gain_after_trim;

	/* Drag-n-Drop */
	void track_canvas_drag_data_received (
	        const Glib::RefPtr<Gdk::DragContext>& context,
	        gint                                  x,
	        gint                                  y,
	        const Gtk::SelectionData&             data,
	        guint                                 info,
	        guint                                 time);

	void drop_paths (
	        const Glib::RefPtr<Gdk::DragContext>& context,
	        gint                                  x,
	        gint                                  y,
	        const Gtk::SelectionData&             data,
	        guint                                 info,
	        guint                                 time);

	void drop_regions (
	        const Glib::RefPtr<Gdk::DragContext>& context,
	        gint                                  x,
	        gint                                  y,
	        const Gtk::SelectionData&             data,
	        guint                                 info,
	        guint                                 time);

	/* audio export */

	enum BounceTarget {
		NewSource,
		NewTrigger,
		ReplaceRange
	};

	int  write_region_selection(RegionSelection&);
	bool write_region (std::string path, boost::shared_ptr<ARDOUR::AudioRegion>);
	void bounce_region_selection (bool with_processing);
	void bounce_range_selection (BounceTarget, bool enable_processing);
	void external_edit_region ();

	int write_audio_selection (TimeSelection&);
	bool write_audio_range (ARDOUR::AudioPlaylist&, const ARDOUR::ChanCount& channels, std::list<ARDOUR::TimelineRange>&);

	void write_selection ();

	uint32_t selection_op_cmd_depth;
	uint32_t selection_op_history_it;

	std::list<XMLNode*> selection_op_history; /* used in *_reversible_selection_op */
	std::list<XMLNode*> before; /* used in *_reversible_command */

	void update_title ();
	void update_title_s (const std::string & snapshot_name);

	void instant_save ();
	bool no_save_instant;

	boost::shared_ptr<ARDOUR::AudioRegion> last_audition_region;

	/* freeze operations */

	ARDOUR::InterThreadInfo freeze_status;
	static void* _freeze_thread (void*);
	void* freeze_thread ();

	void freeze_route ();
	void unfreeze_route ();

	/* duplication */

	void duplicate_range (bool with_dialog);
	void duplicate_regions (float times);

	/** computes the timeline sample (sample) of an event whose coordinates
	 * are in canvas units (pixels, scroll offset included).
	 */
	samplepos_t canvas_event_sample (GdkEvent const*, double* px = 0, double* py = 0) const;

	/** computes the timeline position for an event whose coordinates
	 * are in canvas units (pixels, scroll offset included). The time
	 * domain used by the return value will match ::default_time_domain()
	 * at the time of calling.
	 */
	Temporal::timepos_t canvas_event_time (GdkEvent const*, double* px = 0, double* py = 0) const;

	/** computes the timeline sample (sample) of an event whose coordinates
	 * are in window units (pixels, no scroll offset).
	 */
	samplepos_t window_event_sample (GdkEvent const*, double* px = 0, double* py = 0) const;

	/* returns false if mouse pointer is not in track or marker canvas
	 */
	bool mouse_sample (samplepos_t&, bool& in_track_canvas) const;

	TimeFXDialog* current_timefx;
	static void* timefx_thread (void* arg);
	void do_timefx ();

	int time_stretch (RegionSelection&, Temporal::ratio_t const & fraction);
	int pitch_shift (RegionSelection&, float cents);
	void pitch_shift_region ();

	/* editor-mixer strip */

	MixerStrip *current_mixer_strip;
	bool show_editor_mixer_when_tracks_arrive;
	Gtk::VBox current_mixer_strip_vbox;
	void cms_new (boost::shared_ptr<ARDOUR::Route>);
	void current_mixer_strip_hidden ();

#ifdef __APPLE__
	void ensure_all_elements_drawn ();
#endif
	/* nudging tracks */

	void nudge_track (bool use_edit_point, bool forwards);

	static const int32_t default_width = 995;
	static const int32_t default_height = 765;

	/* nudge */

	ArdourWidgets::ArdourButton      nudge_forward_button;
	ArdourWidgets::ArdourButton      nudge_backward_button;
	Gtk::HBox        nudge_hbox;
	Gtk::VBox        nudge_vbox;
	AudioClock*       nudge_clock;

	bool nudge_forward_release (GdkEventButton*);
	bool nudge_backward_release (GdkEventButton*);

	/* audio filters */

	void apply_filter (ARDOUR::Filter&, std::string cmd, ProgressReporter* progress = 0);

	Command* apply_midi_note_edit_op_to_region (ARDOUR::MidiOperator& op, MidiRegionView& mrv);
	void apply_midi_note_edit_op (ARDOUR::MidiOperator& op, const RegionSelection& rs);

	/* plugin setup */
	int plugin_setup (boost::shared_ptr<ARDOUR::Route>, boost::shared_ptr<ARDOUR::PluginInsert>, ARDOUR::Route::PluginSetupOptions);

	/* handling cleanup */

	int playlist_deletion_dialog (boost::shared_ptr<ARDOUR::Playlist>);

	PBD::ScopedConnectionList session_connections;
	PBD::ScopedConnection tempo_map_connection;

	/* tracking step changes of track height */

	TimeAxisView* current_stepping_trackview;
	PBD::microseconds_t last_track_height_step_timestamp;
	gint track_height_step_timeout();
	sigc::connection step_timeout;

	TimeAxisView* entered_track;
	/** If the mouse is over a RegionView or one of its child canvas items, this is set up
	    to point to the RegionView.  Otherwise it is 0.
	*/
	RegionView*   entered_regionview;

	std::vector<EnterContext> _enter_stack;

	bool clear_entered_track;
	bool left_track_canvas (GdkEventCrossing*);
	bool entered_track_canvas (GdkEventCrossing*);
	void set_entered_track (TimeAxisView*);
	void set_entered_regionview (RegionView*);
	gint left_automation_track ();

	void reset_canvas_action_sensitivity (bool);
	void set_gain_envelope_visibility ();
	void set_region_gain_visibility (RegionView*);
	void toggle_gain_envelope_active ();
	void reset_region_gain_envelopes ();

	void session_state_saved (std::string);

	Glib::RefPtr<Gtk::Action>              undo_action;
	Glib::RefPtr<Gtk::Action>              redo_action;
	Glib::RefPtr<Gtk::Action>              alternate_redo_action;
	Glib::RefPtr<Gtk::Action>              alternate_alternate_redo_action;
	Glib::RefPtr<Gtk::Action>              selection_undo_action;
	Glib::RefPtr<Gtk::Action>              selection_redo_action;

	void history_changed ();

	Editing::EditPoint _edit_point;

	ArdourWidgets::ArdourDropdown edit_point_selector;
	void build_edit_point_menu();

	void set_edit_point_preference (Editing::EditPoint ep, bool force = false);
	void cycle_edit_point (bool with_marker);
	void set_edit_point ();
	void edit_point_selection_done (Editing::EditPoint);
	void edit_point_chosen (Editing::EditPoint);
	Glib::RefPtr<Gtk::RadioAction> edit_point_action (Editing::EditPoint);
	std::vector<std::string> edit_point_strings;
	std::vector<std::string> edit_mode_strings;

	void selected_marker_moved (ARDOUR::Location*);

	bool get_edit_op_range (Temporal::timepos_t& start, Temporal::timepos_t& end) const;

	void get_regions_at (RegionSelection&, Temporal::timepos_t const & where, const TrackViewList& ts) const;
	void get_regions_after (RegionSelection&, Temporal::timepos_t const & where, const TrackViewList& ts) const;

	RegionSelection get_regions_from_selection_and_edit_point (Editing::EditIgnoreOption = Editing::EDIT_IGNORE_NONE,
	                                                           bool use_context_click = false,
	                                                           bool from_outside_canvas = false);
	RegionSelection get_regions_from_selection_and_entered () const;

	void start_updating_meters ();
	void stop_updating_meters ();
	bool meters_running;

	void select_next_stripable (bool routes_only = true);
	void select_prev_stripable (bool routes_only = true);

	Temporal::timepos_t snap_to_minsec (Temporal::timepos_t const & start,
	                                    Temporal::RoundMode   direction,
	                                    ARDOUR::SnapPref    gpref);

	Temporal::timepos_t snap_to_cd_frames (Temporal::timepos_t const & start,
	                                       Temporal::RoundMode   direction,
	                                       ARDOUR::SnapPref    gpref);

	Temporal::timepos_t snap_to_timecode (Temporal::timepos_t const & start,
	                                      Temporal::RoundMode   direction,
	                                      ARDOUR::SnapPref    gpref);

	Temporal::timepos_t snap_to_grid (Temporal::timepos_t const & start,
	                                  Temporal::RoundMode   direction,
	                                  ARDOUR::SnapPref    gpref);

	void snap_to_internal (Temporal::timepos_t& first,
	                       Temporal::RoundMode    direction = Temporal::RoundNearest,
	                       ARDOUR::SnapPref     gpref = ARDOUR::SnapToAny_Visual,
	                       bool                 ensure_snap = false);

	void timecode_snap_to_internal (Temporal::timepos_t & first,
	                                Temporal::RoundMode   direction = Temporal::RoundNearest,
	                                bool                for_mark  = false);

	Temporal::timepos_t snap_to_marker (Temporal::timepos_t const & presnap,
	                                    Temporal::RoundMode direction = Temporal::RoundNearest);

	RhythmFerret* rhythm_ferret;

	void fit_tracks (TrackViewList &);
	void fit_selection ();
	void set_track_height (Height);

	void _remove_tracks ();
	bool idle_remove_tracks ();
	void toggle_tracks_active ();

	bool _have_idled;
	int resize_idle_id;
	static gboolean _idle_resize (gpointer);
	bool idle_resize();
	int32_t _pending_resize_amount;
	TimeAxisView* _pending_resize_view;

	void visible_order_range (int*, int*) const;

	void located ();

	/** true if we've made a locate request that hasn't yet been processed */
	bool _pending_locate_request;

	/** if true, there is a pending Session locate which is the initial one when loading a session;
	    we need to know this so that we don't (necessarily) set the viewport to show the playhead
	    initially.
	*/
	bool _pending_initial_locate;

	Gtk::HBox _summary_hbox;
	EditorSummary* _summary;

	void region_view_added (RegionView*);
	void region_view_removed ();

	EditorGroupTabs* _group_tabs;
	void fit_route_group (ARDOUR::RouteGroup*);

	void step_edit_status_change (bool);
	void start_step_editing ();
	void stop_step_editing ();
	bool check_step_edit ();
	sigc::connection step_edit_connection;

	double _last_motion_y;

	RegionLayeringOrderEditor* layering_order_editor;
	void update_region_layering_order_editor ();

	/** Track that was the source for the last cut/copy operation.  Used as a place
	    to paste things iff there is no selected track.
	*/
	TimeAxisView* _last_cut_copy_source_track;

	/** true if a change in Selection->regions should change the selection in the region list.
	    See EditorRegions::selection_changed.
	*/
	bool _region_selection_change_updates_region_list;

	void setup_fade_images ();
	std::map<ARDOUR::FadeShape, Gtk::Image*> _xfade_in_images;
	std::map<ARDOUR::FadeShape, Gtk::Image*> _xfade_out_images;

	Gtk::MenuItem& action_menu_item (std::string const &);
	void action_pre_activated (Glib::RefPtr<Gtk::Action> const &);

	MouseCursors* _cursors;

	void follow_mixer_selection ();
	bool _following_mixer_selection;

	/* RTAV Automation display option */
	void toggle_show_touched_automation ();
	void set_show_touched_automation (bool);
	bool _show_touched_automation;

	int time_fx (ARDOUR::RegionList&, Temporal::ratio_t ratio, bool pitching);
	void note_edit_done (int, EditNoteDialog*);
	void toggle_sound_midi_notes ();

	/** Flag for a bit of a hack wrt control point selection; see set_selected_control_point_from_click */
	bool _control_point_toggled_on_press;

	/** This is used by TimeAxisView to keep a track of the TimeAxisView that is currently being
	    stepped in height using ScrollZoomVerticalModifier+Scrollwheel.  When a scroll event
	    occurs, we do the step on this _stepping_axis_view if it is non-0 (and we set up this
	    _stepping_axis_view with the TimeAxisView underneath the mouse if it is 0).  Then Editor
	    resets _stepping_axis_view when the modifier key is released.  In this (hacky) way,
	    pushing the modifier key and moving the scroll wheel will operate on the same track
	    until the key is released (rather than skipping about to whatever happens to be
	    underneath the mouse at the time).
	*/
	TimeAxisView* _stepping_axis_view;
	void zoom_vertical_modifier_released();

	void bring_in_callback (Gtk::Label*, uint32_t n, uint32_t total, std::string name);
	void update_bring_in_message (Gtk::Label* label, uint32_t n, uint32_t total, std::string name);
	void bring_all_sources_into_session ();

	QuantizeDialog* quantize_dialog;
	MainMenuDisabler* _main_menu_disabler;

	/* MIDI actions, proxied to selected MidiRegionView(s) */
	void midi_action (void (MidiRegionView::*method)());
	std::vector<MidiRegionView*> filter_to_unique_midi_region_views (RegionSelection const & ms) const;

	/* private helper functions to help with registering region actions */

	Glib::RefPtr<Gtk::Action> register_region_action (Glib::RefPtr<Gtk::ActionGroup> group, Editing::RegionActionTarget, char const* name, char const* label, sigc::slot<void> slot);
	void register_toggle_region_action (Glib::RefPtr<Gtk::ActionGroup> group, Editing::RegionActionTarget, char const* name, char const* label, sigc::slot<void> slot);

	Glib::RefPtr<Gtk::Action> reg_sens (Glib::RefPtr<Gtk::ActionGroup> group, char const* name, char const* label, sigc::slot<void> slot);
	void toggle_reg_sens (Glib::RefPtr<Gtk::ActionGroup> group, char const* name, char const* label, sigc::slot<void> slot);
	void radio_reg_sens (Glib::RefPtr<Gtk::ActionGroup> action_group, Gtk::RadioAction::Group& radio_group, char const* name, char const* label, sigc::slot<void> slot);

	void remove_gap_marker_callback (Temporal::timepos_t at, Temporal::timecnt_t distance);

	friend class Drag;
	friend class RegionCutDrag;
	friend class RegionDrag;
	friend class RegionMoveDrag;
	friend class RegionRippleDrag;
	friend class TrimDrag;
	friend class BBTRulerDrag;
	friend class MeterMarkerDrag;
	friend class TempoMarkerDrag;
	friend class CursorDrag;
	friend class FadeInDrag;
	friend class FadeOutDrag;
	friend class MarkerDrag;
	friend class RegionGainDrag;
	friend class ControlPointDrag;
	friend class LineDrag;
	friend class RubberbandSelectDrag;
	friend class EditorRubberbandSelectDrag;
	friend class TimeFXDrag;
	friend class ScrubDrag;
	friend class SelectionDrag;
	friend class RangeMarkerBarDrag;
	friend class MouseZoomDrag;
	friend class RegionCreateDrag;
	friend class RegionMotionDrag;
	friend class RegionInsertDrag;
	friend class VideoTimeLineDrag;

	friend class EditorSummary;
	friend class EditorGroupTabs;

	friend class EditorRoutes;
	friend class RhythmFerret;
};

#endif /* __ardour_editor_h__ */
