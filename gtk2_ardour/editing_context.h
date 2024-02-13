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

#include <queue>

#include "pbd/signals.h"

#include "temporal/timeline.h"

#include "ardour/midi_operator.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_spacer.h"

#include "axis_provider.h"
#include "editing.h"
#include "editor_items.h"
#include "selection.h"

using ARDOUR::samplepos_t;
using ARDOUR::samplecnt_t;

namespace Temporal {
	class TempoMap;
}

class XMLNode;

class CursorContext;
class DragManager;
class EditorCursor;
class EditNoteDialog;
class MidiRegionView;
class MidiView;
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

	EditingContext (std::string const &);
	~EditingContext ();

	std::string editor_name() const { return _name; }

	void set_session (ARDOUR::Session*);

	Temporal::TimeDomain time_domain () const;


	struct TempoMapScope {
		TempoMapScope (EditingContext& context, std::shared_ptr<Temporal::TempoMap> map) 
			: ec (context)
		{
			old_map = ec.start_local_tempo_map (map);
		}
		~TempoMapScope () {
			ec.end_local_tempo_map (old_map);
		}
		EditingContext& ec;
		std::shared_ptr<Temporal::TempoMap const> old_map;
	};

	DragManager* drags () const {
		return _drags;
	}

	bool drag_active () const;
	bool preview_video_drag_active () const;

	virtual ArdourCanvas::Duple upper_left() const { return ArdourCanvas::Duple (0, 0); }

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
	virtual Temporal::timecnt_t get_nudge_distance (Temporal::timepos_t const & pos, Temporal::timecnt_t& next) const = 0;

	Temporal::timecnt_t relative_distance (Temporal::timepos_t const & origin, Temporal::timecnt_t const & duration, Temporal::TimeDomain domain);
	Temporal::timecnt_t snap_relative_time_to_relative_time (Temporal::timepos_t const & origin, Temporal::timecnt_t const & x, bool ensure_snap) const;

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

	samplecnt_t get_current_zoom () const { return samples_per_pixel; }

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

	/** computes the timeline position for an event whose coordinates
	 * are in canvas units (pixels, scroll offset included). The time
	 * domain used by the return value will match ::default_time_domain()
	 * at the time of calling.
	 */
	Temporal::timepos_t canvas_event_time (GdkEvent const*, double* px = 0, double* py = 0) const;

	/** computes the timeline sample (sample) of an event whose coordinates
	 * are in canvas units (pixels, scroll offset included).
	 */
	samplepos_t canvas_event_sample (GdkEvent const * event, double* pcx = nullptr, double* pcy = nullptr) const;

	virtual bool canvas_note_event (GdkEvent* event, ArdourCanvas::Item*) = 0;

	virtual Temporal::Beats get_grid_type_as_beats (bool& success, Temporal::timepos_t const & position) const = 0;
	virtual Temporal::Beats get_draw_length_as_beats (bool& success, Temporal::timepos_t const & position) const = 0;

	virtual int32_t get_grid_beat_divisions (Editing::GridType gt) const = 0;
	virtual int32_t get_grid_music_divisions (Editing::GridType gt, uint32_t event_state) const = 0;

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
	                      bool                  ensure_snap = false) const;

	virtual void snap_to_with_modifier (Temporal::timepos_t & first,
	                                    GdkEvent const*       ev,
	                                    Temporal::RoundMode   direction = Temporal::RoundNearest,
	                                    ARDOUR::SnapPref      gpref = ARDOUR::SnapToAny_Visual,
	                                    bool ensure_snap = false) const;

	virtual Temporal::timepos_t snap_to_bbt (Temporal::timepos_t const & start,
	                                         Temporal::RoundMode   direction,
	                                         ARDOUR::SnapPref    gpref) const;

	virtual double get_y_origin () const = 0;
	virtual void reset_x_origin (samplepos_t) = 0;
	virtual void reset_y_origin (double) = 0;

	virtual void set_zoom_focus (Editing::ZoomFocus) = 0;
	virtual Editing::ZoomFocus get_zoom_focus () const = 0;
	virtual void reset_zoom (samplecnt_t) = 0;
	virtual void reposition_and_zoom (samplepos_t, double) = 0;

	sigc::signal<void> ZoomChanged;

	virtual Selection& get_selection() const { return *selection; }
	virtual Selection& get_cut_buffer () const { return *cut_buffer; }

	/** Set the mouse mode (gain, object, range, timefx etc.)
	 * @param m Mouse mode (defined in editing_syms.h)
	 * @param force Perform the effects of the change even if no change is required
	 * (ie even if the current mouse mode is equal to @p m)
	 */
	virtual void set_mouse_mode (Editing::MouseMode, bool force = false);
	/** Step the mouse mode onto the next or previous one.
	 * @param next true to move to the next, otherwise move to the previous
	 */
	virtual void step_mouse_mode (bool next) = 0;
	/** @return The current mouse mode (gain, object, range, timefx etc.)
	 * (defined in editing_syms.h)
	 */
	Editing::MouseMode current_mouse_mode () const { return mouse_mode; }
	/** @return Whether the current mouse mode is an "internal" editing mode. */
	virtual bool internal_editing() const = 0;

	virtual Gdk::Cursor* get_canvas_cursor () const;
	static MouseCursors const* cursors () {
		return _cursors;
	}
	virtual VerboseCursor* verbose_cursor () const {
		return _verbose_cursor;
	}

	virtual void set_snapped_cursor_position (Temporal::timepos_t const & pos) = 0;

	static sigc::signal<void> DropDownKeys;

	PBD::Signal0<void> SnapChanged;
	PBD::Signal0<void> MouseModeChanged;

	/* MIDI actions, proxied to selected MidiRegionView(s) */
	ARDOUR::Quantize* get_quantize_op ();
	void apply_midi_note_edit_op (ARDOUR::MidiOperator& op, const RegionSelection& rs);
	void midi_action (void (MidiView::*method)());
	std::vector<MidiView*> filter_to_unique_midi_region_views (RegionSelection const & ms) const;

	void quantize_region ();
	void transform_region ();
	void legatize_region (bool shrink_only);
	void transpose_region ();

	static void register_midi_actions (Gtkmm2ext::Bindings*);

	ArdourCanvas::Rectangle* rubberband_rect;

	virtual ArdourCanvas::Container* get_noscroll_group() const = 0;
	virtual ArdourCanvas::ScrollGroup* get_hscroll_group () const = 0;
	virtual ArdourCanvas::ScrollGroup* get_cursor_scroll_group () const = 0;
	virtual bool canvas_playhead_cursor_event (GdkEvent* event, ArdourCanvas::Item*) { return false; }

	bool typed_event (ArdourCanvas::Item*, GdkEvent*, ItemType);

	void set_horizontal_position (double);
	double horizontal_position () const;

	virtual samplecnt_t current_page_samples() const = 0;

	virtual ArdourCanvas::GtkCanvasViewport* get_canvas_viewport() const = 0;
	virtual ArdourCanvas::Canvas* get_canvas() const = 0;

	virtual size_t push_canvas_cursor (Gdk::Cursor*);
	virtual void pop_canvas_cursor ();

	virtual void mouse_mode_toggled (Editing::MouseMode) = 0;

	bool on_velocity_scroll_event (GdkEventScroll*);

  protected:
	std::string _name;

	static Glib::RefPtr<Gtk::ActionGroup> _midi_actions;

	/* Cursor stuff.  Do not use directly, use via CursorContext. */
	friend class CursorContext;
	std::vector<Gdk::Cursor*> _cursor_stack;
	virtual void set_canvas_cursor (Gdk::Cursor*);

	Editing::GridType  pre_internal_grid_type;
	Editing::SnapMode  pre_internal_snap_mode;
	Editing::GridType  internal_grid_type;
	Editing::SnapMode  internal_snap_mode;

	static std::vector<std::string> grid_type_strings;

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
	static void _draw_length_chosen (Editing::GridType);

	void draw_velocity_selection_done (int);
	void draw_velocity_chosen (int);
	static void _draw_velocity_chosen (int);

	void draw_channel_selection_done (int);
	void draw_channel_chosen (int);
	static void _draw_channel_chosen (int);

	DragManager* _drags;

	ArdourWidgets::ArdourButton snap_mode_button;

	virtual void mark_region_boundary_cache_dirty () {}
	virtual void update_tempo_based_rulers () {};
	virtual void show_rulers_for_grid () {};

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

	static MouseCursors* _cursors;

	VerboseCursor* _verbose_cursor;

	samplecnt_t        samples_per_pixel;
	Editing::ZoomFocus zoom_focus;

	Temporal::timepos_t _snap_to_bbt (Temporal::timepos_t const & start,
	                                  Temporal::RoundMode   direction,
	                                  ARDOUR::SnapPref    gpref,
	                                  Editing::GridType   grid_type) const;

	virtual Temporal::timepos_t snap_to_grid (Temporal::timepos_t const & start,
	                                          Temporal::RoundMode   direction,
	                                          ARDOUR::SnapPref    gpref) const = 0;

	virtual void snap_to_internal (Temporal::timepos_t& first,
	                               Temporal::RoundMode    direction = Temporal::RoundNearest,
	                               ARDOUR::SnapPref     gpref = ARDOUR::SnapToAny_Visual,
	                               bool                 ensure_snap = false) const = 0;

	void check_best_snap (Temporal::timepos_t const & presnap, Temporal::timepos_t &test, Temporal::timepos_t &dist, Temporal::timepos_t &best) const;
	virtual double visible_canvas_width() const = 0;

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
	uint32_t bbt_bar_helper_on;

	uint32_t count_bars (Temporal::Beats const & start, Temporal::Beats const & end) const;
	void compute_bbt_ruler_scale (samplepos_t lower, samplepos_t upper);

	double _visible_canvas_width;
	double _visible_canvas_height; ///< height of the visible area of the track canvas

	QuantizeDialog* quantize_dialog;

	friend class TempoMapScope;
	virtual std::shared_ptr<Temporal::TempoMap const> start_local_tempo_map (std::shared_ptr<Temporal::TempoMap>);
	virtual void end_local_tempo_map (std::shared_ptr<Temporal::TempoMap const>) { /* no-op by default */ }

	virtual bool button_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) = 0;
	virtual bool button_press_handler_1 (ArdourCanvas::Item*, GdkEvent*, ItemType) = 0;
	virtual bool button_press_handler_2 (ArdourCanvas::Item*, GdkEvent*, ItemType) = 0;
	virtual bool button_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) = 0;
	virtual bool button_press_dispatch (GdkEventButton*) = 0;
	virtual bool button_release_dispatch (GdkEventButton*) = 0;
	virtual bool motion_handler (ArdourCanvas::Item*, GdkEvent*, bool from_autoscroll = false) = 0;
	virtual bool enter_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) = 0;
	virtual bool leave_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) = 0;
	virtual bool key_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) = 0;
	virtual bool key_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType) = 0;

	void popup_note_context_menu (ArdourCanvas::Item*, GdkEvent*);
	Gtk::Menu _note_context_menu;

	static Gtkmm2ext::Bindings* button_bindings;
	XMLNode* button_settings () const;

	virtual RegionSelection region_selection() = 0;

	void edit_notes (MidiView*);
	void note_edit_done (int, EditNoteDialog*);

	PBD::Command* apply_midi_note_edit_op_to_region (ARDOUR::MidiOperator& op, MidiView& mrv);

	void quantize_regions (const RegionSelection& rs);
	void legatize_regions (const RegionSelection& rs, bool shrink_only);
	void transform_regions (const RegionSelection& rs);
	void transpose_regions (const RegionSelection& rs);

	static EditingContext* current_editing_context();
	static void push_editing_context (EditingContext*);
	static void pop_editing_context ();

	/** the adjustment that controls the overall editing vertical scroll position */
	friend class EditorSummary;
	Gtk::Adjustment     vertical_adjustment;
	Gtk::Adjustment     horizontal_adjustment;

	ArdourWidgets::ArdourButton mouse_select_button;
	ArdourWidgets::ArdourButton mouse_timefx_button;
	ArdourWidgets::ArdourButton mouse_grid_button;
	ArdourWidgets::ArdourButton mouse_cut_button;
	ArdourWidgets::ArdourButton mouse_move_button;
	ArdourWidgets::ArdourButton mouse_draw_button;
	ArdourWidgets::ArdourButton mouse_content_button;

	Glib::RefPtr<Gtk::Action> get_mouse_mode_action (Editing::MouseMode m) const;
	void register_mouse_mode_actions ();
	void bind_mouse_mode_buttons ();
	virtual void add_mouse_mode_actions (Glib::RefPtr<Gtk::ActionGroup>) {}

	ArdourWidgets::ArdourButton* snap_button;

	Gtk::HBox snap_box;
	Gtk::HBox grid_box;
	Gtk::HBox draw_box;

	ArdourWidgets::ArdourVSpacer _grid_box_spacer;
	ArdourWidgets::ArdourVSpacer _draw_box_spacer;

	void pack_draw_box ();
	void pack_snap_box ();

	Gtkmm2ext::Bindings* bindings;

	Editing::MouseMode mouse_mode;

	void set_common_editing_state (XMLNode const & node);
	void get_common_editing_state (XMLNode& node) const;

  private:
	static std::queue<EditingContext*> ec_stack;

};


#endif /* __ardour_midi_editing_context_h__ */

