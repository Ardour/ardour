/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef __gtk2_ardour_editor_drag_h_
#define __gtk2_ardour_editor_drag_h_

#include <list>
#include <vector>

#include <gdk/gdk.h>
#include <stdint.h>

#include "ardour/tempo.h"
#include "ardour/types.h"

#include "evoral/ControlList.h"

#include "canvas/types.h"

#include "gtkmm2ext/bindings.h"

#include "cursor_context.h"
#include "editor_items.h"
#include "mouse_cursors.h"
#include "editing.h"
#include "track_selection.h"
#include "region_view.h"

namespace ARDOUR {
	class Location;
	class Region;
	class TempoSection;
}

namespace ArdourCanvas {
	class Item;
	class Line;
	class Rectangle;
	class Lollipop;
}

namespace PBD {
	class StatefulDiffCommand;
}

class PatchChange;
class EditingContext;
class Editor;
class EditorCursor;
class TimeAxisView;
class MidiTimeAxisView;
class Drag;
class NoteBase;
class TimeAxisView;
class RouteTimeAxisView;
class RegionSelection;
class MidiRegionView;
class MidiView;
class MeterMarker;
class ArdourMarker;
class TempoMarker;
class TempoCurve;
class ControlPoint;
class AudioRegionView;
class AutomationLine;
class AutomationTimeAxisView;
class VelocityGhostRegion;

/** Class to manage current drags */
class DragManager
{
public:

	DragManager (EditingContext* e);
	~DragManager ();

	bool motion_handler (GdkEvent *, bool);
	bool mid_drag_key_event (GdkEventKey*);
	void abort ();
	void add (Drag *);
	void set (Drag *, GdkEvent *, Gdk::Cursor* c = MouseCursors::invalid_cursor());
	void start_grab (GdkEvent *, Gdk::Cursor* c = MouseCursors::invalid_cursor());
	bool end_grab (GdkEvent *);
	bool have_item (ArdourCanvas::Item *) const;

	void mark_double_click ();

	/** @return true if an end drag or abort is in progress */
	bool ending () const {
		return _ending;
	}

	bool active () const {
		return !_drags.empty ();
	}

	/** @return current pointer x position in canvas coordinates */
	double current_pointer_x () const {
		return _current_pointer_x;
	}

	/** @return current pointer y position in canvas coordinates */
	double current_pointer_y () const {
		return _current_pointer_y;
	}

	/** @return current pointer sample */
	ARDOUR::samplepos_t current_pointer_sample () const {
		return _current_pointer_time.samples();
	}

	/** @return current pointer time */
	Temporal::timepos_t current_pointer_time () const {
		return _current_pointer_time;
	}

	/** return drag-motion displays video-frame of drag-location */
	bool preview_video () const;

private:
	EditingContext* _editing_context;
	std::list<Drag*> _drags;
	bool _ending; ///< true if end_grab or abort is in progress, otherwise false
	double _current_pointer_x; ///< canvas-coordinate space x of the current pointer
	double _current_pointer_y; ///< canvas-coordinate space y of the current pointer
	Temporal::timepos_t _current_pointer_time; ///< time that the pointer is now at
	bool _old_follow_playhead; ///< state of Editor::follow_playhead() before the drags started
};

/** Abstract base class for dragging of things within the editor */
class Drag
{
public:
	Drag (EditingContext&, ArdourCanvas::Item *, Temporal::TimeDomain td, bool trackview_only = true, bool hide_snapped_cursor = true);
	virtual ~Drag ();

	void set_manager (DragManager* m) {
		_drags = m;
	}

	/** @return the canvas item being dragged */
	ArdourCanvas::Item* item () const {
		return _item;
	}

	Temporal::TimeDomain time_domain() const { return _time_domain; }
	void set_time_domain (Temporal::TimeDomain);

	void swap_grab (ArdourCanvas::Item *, Gdk::Cursor *, uint32_t);
	bool motion_handler (GdkEvent*, bool);
	void abort ();

	Temporal::timepos_t adjusted_time (Temporal::timepos_t const &, GdkEvent const *, bool snap = true) const;
	Temporal::timepos_t adjusted_current_time (GdkEvent const *, bool snap = true) const;

	bool was_double_click() const { return _was_double_click; }
	void set_double_click (bool yn) { _was_double_click = yn; }

	void set_grab_button_anyway (GdkEvent*);

	/** Called to start a grab of an item.
	 *  @param e Event that caused the grab to start.
	 *  @param c Cursor to use, or 0.
	 */
	virtual void start_grab (GdkEvent* e, Gdk::Cursor* c = 0);

	virtual bool end_grab (GdkEvent *);

	/** Called when a drag motion has occurred.
	 *  @param e Event describing the motion.
	 *  @param f true if this is the first movement, otherwise false.
	 */
	virtual void motion (GdkEvent* e, bool f) = 0;

	/** Called when a drag has finished.
	 *  @param e Event describing the finish.
	 *  @param m true if some movement occurred, otherwise false.
	 */
	virtual void finished (GdkEvent* e, bool m) = 0;

	/** Called to abort a drag and return things to how
	 *  they were before it started.
	 *  @param m true if some movement occurred, otherwise false.
	 */
	virtual void aborted (bool m) = 0;

	/** @param m Mouse mode.
	 *  @return true if this drag should happen in this mouse mode.
	 */
	virtual bool active (Editing::MouseMode m) {
		return true;
	}

	bool preview_video () const {
		return _preview_video;
	}

	/** @return minimum number of samples (in x) and pixels (in y) that should be considered a movement */
	std::pair<Temporal::timecnt_t,int> move_threshold () const {
		return std::make_pair (Temporal::timecnt_t (1, Temporal::AudioTime), 1);
	}

	virtual bool allow_vertical_autoscroll () const {
		return true;
	}

	virtual bool allow_horizontal_autoscroll () const {
		return true;
	}

	/** @return true if x movement matters to this drag */
	virtual bool x_movement_matters () const {
		return true;
	}

	/** @return true if y movement matters to this drag */
	virtual bool y_movement_matters () const {
		return true;
	}

	virtual bool mid_drag_key_event (GdkEventKey*)  { return false; }

	bool initially_vertical() const {
		return _initially_vertical;
	}

	/** Set up the _pointer_offset */
	virtual void setup_pointer_offset () {
		_pointer_offset = Temporal::timecnt_t();
	}

	/** Set up the _video_offset - relative to _current_pointer_time */
	virtual void setup_video_offset () {
		/* video offset is always in audio time */
		_video_offset = Temporal::timecnt_t (Temporal::AudioTime);
		_preview_video = false;
	}

	int grab_button() const { return _grab_button; }

protected:

	double grab_x () const {
		return _grab_x;
	}

	double grab_y () const {
		return _grab_y;
	}

	Temporal::timepos_t pixel_to_time (double x) const;

	Temporal::timepos_t raw_grab_time () const {
		return _raw_grab_time;
	}

	ARDOUR::samplepos_t grab_sample () const {
		return _grab_time.samples();
	}

	Temporal::timepos_t grab_time () const {
		return _grab_time;
	}

	double last_pointer_x () const {
		return _last_pointer_x;
	}

	double last_pointer_y () const {
		return _last_pointer_y;
	}

	ARDOUR::samplepos_t last_pointer_sample () const {
		return _last_pointer_time.samples();
	}

	Temporal::timepos_t last_pointer_time () const {
		return _last_pointer_time;
	}

	Temporal::timecnt_t snap_delta (guint const) const;

	double current_pointer_x () const;
	double current_pointer_y () const;

	/* sets snap delta from unsnapped pos */
	void setup_snap_delta (Temporal::timepos_t const & pos);

	std::shared_ptr<ARDOUR::Region> add_midi_region (MidiTimeAxisView*, bool commit);

	void show_verbose_cursor_time (Temporal::timepos_t const &);
	void show_verbose_cursor_duration (Temporal::timepos_t const & , Temporal::timepos_t const & , double xoffset = 0);
	void show_verbose_cursor_text (std::string const &);
	void show_view_preview (Temporal::timepos_t const &);

	EditingContext& editing_context;
	DragManager* _drags;
	ArdourCanvas::Item* _item; ///< our item
	/** Offset from the mouse's position for the drag to the start of the thing that is being dragged */
	Temporal::timecnt_t _pointer_offset;
	Temporal::timecnt_t _video_offset;
	bool _preview_video;
	bool _x_constrained; ///< true if x motion is constrained, otherwise false
	bool _y_constrained; ///< true if y motion is constrained, otherwise false
	bool _was_rolling; ///< true if the session was rolling before the drag started, otherwise false
	ARDOUR::timepos_t _earliest_time_limit; ///< time we cannot drag before (defaults to 0, indicating no such limit)

private:
	bool _trackview_only; ///< true if pointer y value should always be relative to the top of the trackview group
	bool _hide_snapped_cursor; ///< set true of Drag::motion does not call `set_snapped_cursor_position`
	bool _move_threshold_passed; ///< true if the move threshold has been passed, otherwise false
	bool _starting_point_passed; ///< true if we called move () with first_move flag, otherwise false
	bool _initially_vertical; ///< true if after move threshold is passed we appear to be moving vertically; undefined before that
	bool _was_double_click; ///< true if drag initiated by a double click event
	double _grab_x; ///< trackview x of the grab start position
	double _grab_y; ///< y of the grab start position, possibly adjusted if _trackview_only is true
	double _last_pointer_x; ///< trackview x of the pointer last time a motion occurred
	double _last_pointer_y; ///< trackview y of the pointer last time a motion occurred
	Temporal::timepos_t _raw_grab_time; ///< unsnapped time that the mouse was at when start_grab was called, or 0
	Temporal::timepos_t _grab_time; ///< adjusted_time that the mouse was at when start_grab was called, or 0
	Temporal::timepos_t _last_pointer_time; ///< adjusted_time the last time a motion occurred
	Temporal::TimeDomain _time_domain;

	/* difference between some key position's snapped and unsnapped
	 *  samplepos. used for relative snap.
	 */
	Temporal::timecnt_t _snap_delta;
	CursorContext::Handle _cursor_ctx; ///< cursor change context
	bool _constraint_pressed; ///< if the keyboard indicated constraint modifier was pressed on start_grab()
	int _grab_button;

	Gtkmm2ext::Bindings::DragsBlockBindings binding_blocker;
};

/** EditorDrag:
 *
 * a base class for Drags that will need access to the full Editor, not just an
 * Editing Context.
 */
class EditorDrag : public Drag
{
  public:
	EditorDrag (Editor&, ArdourCanvas::Item *, Temporal::TimeDomain td, bool trackview_only = true, bool hide_snapped_cursor = true);
  protected:
	Editor& _editor;
};

class RegionDrag;

/** Container for details about a region being dragged */
class DraggingView
{
public:
	DraggingView (RegionView *, RegionDrag *, TimeAxisView* original_tav);

	RegionView* view; ///< the view
	/** index into RegionDrag::_time_axis_views of the view that this region is currently being displayed on,
	 *  or -1 if it is not visible.
	 */
	int time_axis_view;
	/** layer that this region is currently being displayed on.  This is a double
	    rather than a layer_t as we use fractional layers during drags to allow the user
	    to indicate a new layer to put a region on.
	*/
	double layer;
	double initial_y; ///< the initial y position of the view before any reparenting
	Temporal::timepos_t initial_position; ///< initial position of the region
	Temporal::timepos_t initial_end; ///< initial end position of the region
	samplepos_t anchored_fade_length; ///< fade_length when anchored during drag
	std::shared_ptr<ARDOUR::Playlist> initial_playlist;
	TimeAxisView* initial_time_axis_view;
};

/** Abstract base class for drags that involve region(s) */
class RegionDrag : public EditorDrag, public sigc::trackable
{
public:
	RegionDrag (Editor&, ArdourCanvas::Item *, RegionView *, std::list<RegionView*> const &, Temporal::TimeDomain, bool hide_snapped_cursor = true);
	virtual ~RegionDrag () {}

protected:

	RegionView* _primary; ///< the view that was clicked on (or whatever) to start the drag
	std::list<DraggingView> _views; ///< information about all views that are being dragged

	/** a list of the non-hidden TimeAxisViews sorted by editor order key */
	std::vector<TimeAxisView*> _time_axis_views;
	int find_time_axis_view (TimeAxisView *) const;
	int apply_track_delta (const int start, const int delta, const int skip, const bool distance_only = false) const;

	int _visible_y_low;
	int _visible_y_high;
	uint32_t _ntracks;

	void setup_video_offset ();

	friend class DraggingView;

protected:
	void add_stateful_diff_commands_for_playlists (ARDOUR::PlaylistSet const &);

private:

	void region_going_away (RegionView *);
	PBD::ScopedConnection death_connection;
};

/** Drag the Contents rather than the bounds of a region (i.e. Slip) */
class RegionSlipContentsDrag : public RegionDrag
{
public:

	RegionSlipContentsDrag (Editor&, ArdourCanvas::Item *, RegionView *, std::list<RegionView*> const &, Temporal::TimeDomain td);
	virtual ~RegionSlipContentsDrag () {}

	virtual void start_grab (GdkEvent *, Gdk::Cursor *);
	virtual void motion (GdkEvent *, bool);
	virtual void finished (GdkEvent *, bool);
	virtual void aborted (bool);
};

/** Brush on a region to repeat it */
class RegionBrushDrag : public RegionDrag
{
public:
	RegionBrushDrag (Editor&, ArdourCanvas::Item *, RegionView *, std::list<RegionView*> const &, Temporal::TimeDomain td);
	virtual ~RegionBrushDrag () {}

	virtual void start_grab (GdkEvent *, Gdk::Cursor *);
	virtual void motion (GdkEvent *, bool);
	virtual void finished (GdkEvent *, bool);
	virtual void aborted (bool);
private:
	typedef std::set<Temporal::timepos_t> TimePositionSet;
	TimePositionSet _already_pasted;
};

/** Drags involving region motion from somewhere */
class RegionMotionDrag : public RegionDrag
{
public:

	RegionMotionDrag (Editor&, ArdourCanvas::Item *, RegionView *, std::list<RegionView*> const &, Temporal::TimeDomain td);
	virtual ~RegionMotionDrag () {}

	virtual void start_grab (GdkEvent *, Gdk::Cursor *);
	virtual void motion (GdkEvent *, bool);
	virtual void finished (GdkEvent *, bool);
	virtual void aborted (bool);

	/** @return true if the regions being `moved' came from somewhere on the canvas;
	 *  false if they came from outside (e.g. from the region list).
	 */
	virtual bool regions_came_from_canvas () const = 0;

protected:

	double compute_x_delta (GdkEvent const *, Temporal::timepos_t &);
	virtual bool y_movement_allowed (int, double, int skip_invisible = 0) const;
	void collect_ripple_views ();

	bool _ignore_video_lock;
	Temporal::timepos_t _last_position; ///< last position of the thing being dragged
	double _total_x_delta;
	int _last_pointer_time_axis_view;
	double _last_pointer_layer;
	std::vector<ArdourMarker*> ripple_markers;

private:
	uint32_t _ndropzone;
	uint32_t _pdropzone;
	uint32_t _ddropzone;
};


/** Drags to move (or copy) regions that are already shown in the GUI to
 *  somewhere different.
 */
class RegionMoveDrag : public RegionMotionDrag
{
public:
	RegionMoveDrag (Editor&, ArdourCanvas::Item *, RegionView *, std::list<RegionView*> const &, bool, Temporal::TimeDomain);
	virtual ~RegionMoveDrag () {}

	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	void clear_draggingview_list ();

	bool regions_came_from_canvas () const {
		return true;
	}

	std::pair<Temporal::timecnt_t,int> move_threshold () const {
		return std::make_pair (Temporal::timecnt_t (4, Temporal::AudioTime), 4);
	}

	void setup_pointer_offset ();

private:
	void finished_no_copy (
		bool const,
		bool const,
		Temporal::timepos_t const &,
		int32_t const ev_state
		);

	void finished_copy (
		bool const,
		bool const,
		Temporal::timepos_t const &,
		int32_t const ev_state
		);

	RegionView* insert_region_into_playlist (
		std::shared_ptr<ARDOUR::Region>,
		RouteTimeAxisView*,
		ARDOUR::layer_t,
		Temporal::timepos_t const &,
		ARDOUR::PlaylistSet&
		);

	void remove_region_from_playlist (
		std::shared_ptr<ARDOUR::Region>,
		std::shared_ptr<ARDOUR::Playlist>,
		ARDOUR::PlaylistSet& modified_playlists
		);


	void collect_new_region_view (RegionView *);
	RouteTimeAxisView* create_destination_time_axis (std::shared_ptr<ARDOUR::Region>, TimeAxisView* original);

	bool _copy;
	RegionView* _new_region_view;
};

/** Drag to insert a region from somewhere */
class RegionInsertDrag : public RegionMotionDrag
{
public:
	RegionInsertDrag (Editor&, std::shared_ptr<ARDOUR::Region>, RouteTimeAxisView*, Temporal::timepos_t const &, Temporal::TimeDomain);

	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool regions_came_from_canvas () const {
		return false;
	}
};

/** "Drag" to cut a region (action only on button release) */
class RegionCutDrag : public EditorDrag
{
public:
	RegionCutDrag (Editor&, ArdourCanvas::Item*, samplepos_t);
	~RegionCutDrag ();

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent*, bool);
	void finished (GdkEvent*, bool);
	void aborted (bool);
};

/** Drags to create regions */
class RegionCreateDrag : public EditorDrag
{
public:
	RegionCreateDrag (Editor&, ArdourCanvas::Item *, TimeAxisView *);

	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

private:
	MidiTimeAxisView* _view;
	std::shared_ptr<ARDOUR::Region> _region;
};

/** Drags to resize MIDI notes */
class NoteResizeDrag : public Drag
{
public:
	NoteResizeDrag (EditingContext&, ArdourCanvas::Item *);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

private:
	MidiRegionView*     region;
	bool                relative;
	bool                at_front;
	bool                _was_selected;
	double              _snap_delta;
};

/** Drags to move MIDI notes */
class NoteDrag : public Drag
{
public:
	NoteDrag (EditingContext&, ArdourCanvas::Item*);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	void setup_pointer_offset ();

	bool allow_vertical_autoscroll () const {
		return false;
	}

private:

	Temporal::timecnt_t total_dx (GdkEvent * event) const; // total movement in quarter notes
	int8_t total_dy () const;

	MidiView* _region;
	NoteBase* _primary;
	Temporal::timecnt_t _cumulative_dx;
	double _cumulative_dy;
	Temporal::timepos_t  _earliest; // earliest note in note selection
	bool   _was_selected;
	double _note_height;
	bool   _copy;
};

class NoteCreateDrag : public Drag
{
public:
	NoteCreateDrag (EditingContext&, ArdourCanvas::Item *, MidiView *);
	~NoteCreateDrag ();

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool active (Editing::MouseMode mode) {
		return mode == Editing::MouseDraw || mode == Editing::MouseContent;
	}

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return false;
	}

private:
	double y_to_region (double) const;
	Temporal::Beats round_to_grid (Temporal::timepos_t const & pos, GdkEvent const * event) const;

	/** @return minimum number of samples (in x) and pixels (in y) that should be considered a movement */
	std::pair<Temporal::timecnt_t,int> move_threshold () const {
		return std::make_pair (Temporal::timecnt_t (0, Temporal::AudioTime), 0);
	}

	MidiView* _midi_view;
	ArdourCanvas::Rectangle* _drag_rect;
	Temporal::timepos_t _note[2];
};

class HitCreateDrag : public Drag
{
public:
	HitCreateDrag (EditingContext&, ArdourCanvas::Item *, MidiView *);
	~HitCreateDrag ();

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool) {}
	void finished (GdkEvent *, bool);
	void aborted (bool) {}

	bool active (Editing::MouseMode mode) {
		return mode == Editing::MouseDraw || mode == Editing::MouseContent;
	}

	bool y_movement_matters () const {
		return false;
	}

private:
	double y_to_region (double) const;

	/** @return minimum number of samples (in x) and pixels (in y) that should be considered a movement */
	std::pair<Temporal::timecnt_t,int> move_threshold () const {
		return std::make_pair (Temporal::timecnt_t::zero (Temporal::AudioTime), 0);
	}

	MidiView* _midi_view;
	Temporal::timepos_t _last_pos;
	double          _y;

};

/** Drag to move MIDI patch changes */
class PatchChangeDrag : public Drag
{
public:
	PatchChangeDrag (EditingContext&, PatchChange *, MidiView *);

	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool y_movement_matters () const {
		return false;
	}

	bool allow_vertical_autoscroll () const {
		return false;
	}

	void setup_pointer_offset ();

private:
	MidiView* _region_view;
	PatchChange* _patch_change;
	double _cumulative_dx;
};

/** Container for details about audio regions being dragged along with video */
class AVDraggingView
{
public:
	AVDraggingView (RegionView *);

	RegionView* view; ///< the view
	samplepos_t initial_position; ///< initial position of the region
};

/** Drag of video offset */
class VideoTimeLineDrag : public EditorDrag
{
public:
	VideoTimeLineDrag (Editor&e, ArdourCanvas::Item *i);

	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);

	bool y_movement_matters () const {
		return false;
	}

	bool allow_vertical_autoscroll () const {
		return false;
	}

	void aborted (bool);

protected:
	std::list<AVDraggingView> _views; ///< information about all audio that are being dragged along

private:
	ARDOUR::sampleoffset_t _startdrag_video_offset;
	ARDOUR::sampleoffset_t _max_backwards_drag;
	bool _stuck;
};

/** Drag to trim region(s) */
class TrimDrag : public RegionDrag
{
public:
	enum Operation {
		StartTrim,
		EndTrim
	};

	TrimDrag (Editor&, ArdourCanvas::Item *, RegionView*, std::list<RegionView*> const &, Temporal::TimeDomain td, bool preserve_fade_anchor = false);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool y_movement_matters () const {
		return false;
	}

	bool allow_vertical_autoscroll () const {
		return false;
	}

	void setup_pointer_offset ();

private:

	Operation _operation;

	bool _preserve_fade_anchor;
	bool _jump_position_when_done;

	std::vector<RegionView::DisplaySuspender> suspenders;
};

/** Meter marker drag */
class MeterMarkerDrag : public EditorDrag
{
public:
	MeterMarkerDrag (Editor&, ArdourCanvas::Item *, bool);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return false;
	}

	void setup_pointer_offset ();

private:
	MeterMarker* _marker;
	Temporal::TempoMap::WritableSharedPtr map;
	Temporal::superclock_t initial_sclock;

	bool _movable;
	Editing::GridType _old_grid_type;
	Editing::SnapMode _old_snap_mode;
	XMLNode* before_state;
};

/** Tempo curve drag */
class TempoCurveDrag : public EditorDrag
{
public:
	TempoCurveDrag (Editor&, ArdourCanvas::Item*);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

private:
	Temporal::TempoPoint* point;
	double initial_bpm;
	Temporal::TempoMap::WritableSharedPtr map;
	XMLNode* _before_state;
};

/** Tempo marker drag */
class TempoMarkerDrag : public EditorDrag
{
public:
	TempoMarkerDrag (Editor&, ArdourCanvas::Item *);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return true;
	}

	void setup_pointer_offset ();

private:
	TempoMarker* _marker;
	Temporal::TempoPoint const * _real_section;
	Temporal::TempoMap::WritableSharedPtr map;

	bool _movable;
	double _grab_bpm;
	Temporal::Beats _grab_qn;
	XMLNode* _before_state;
};

/** Tempo marker drag */
class BBTMarkerDrag : public EditorDrag
{
public:
	BBTMarkerDrag (Editor&, ArdourCanvas::Item *);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return false;
	}

	void setup_pointer_offset ();

private:
	BBTMarker* _marker;
	Temporal::MusicTimePoint const * _point;
	Temporal::TempoMap::WritableSharedPtr map;

	XMLNode* _before_state;
};

class MappingEndDrag : public EditorDrag
{
public:
	MappingEndDrag (Editor&, ArdourCanvas::Item *, Temporal::TempoMap::WritableSharedPtr&, Temporal::TempoPoint&, Temporal::TempoPoint& after, XMLNode& before_state);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool allow_horizontal_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return false;
	}

	void setup_pointer_offset ();

private:
	Temporal::TempoPoint& _tempo;
	Temporal::TempoPoint& _after;
	double _grab_bpm;
	Temporal::TempoMap::WritableSharedPtr map;

	XMLNode* _before_state;
	bool     _drag_valid;
};

class MappingTwistDrag : public EditorDrag
{
public:
	MappingTwistDrag (Editor&, ArdourCanvas::Item *, Temporal::TempoMap::WritableSharedPtr&,
	                  Temporal::TempoPoint& prev,
	                  Temporal::TempoPoint& focus,
	                  Temporal::TempoPoint& next,
	                  XMLNode&,
	                  bool ramped);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool allow_horizontal_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return false;
	}

	void setup_pointer_offset ();

private:
	Temporal::TempoPoint& prev;
	Temporal::TempoPoint& focus;
	Temporal::TempoPoint& next;
	Temporal::TempoMap::WritableSharedPtr map;

	double direction;
	double delta;
	double initial_focus_npm;
	double initial_pre_npm;

	XMLNode* _before_state;
	bool     _drag_valid;
	bool     _do_ramp;
};


/** tempo curve twist drag */
class TempoTwistDrag : public EditorDrag
{
public:
	TempoTwistDrag (Editor&, ArdourCanvas::Item *);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return true;
	}

	void setup_pointer_offset ();

private:
	Temporal::Beats _grab_qn;
	Temporal::TempoPoint* _tempo;
	Temporal::TempoPoint const * _next_tempo;
	Temporal::TempoMap::WritableSharedPtr map;
	bool _drag_valid;
	XMLNode* _before_state;
};

/** tempo curve twist drag */
class TempoEndDrag : public EditorDrag
{
public:
	TempoEndDrag (Editor&, ArdourCanvas::Item *);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return true;
	}

	void setup_pointer_offset ();

private:
	Temporal::Beats _grab_qn;
	Temporal::TempoPoint * _tempo;
	Temporal::TempoPoint * previous_tempo;
	Temporal::TempoMap::WritableSharedPtr map;

	XMLNode* _before_state;
	bool _drag_valid;
};

/** Drag of the playhead cursor */
class CursorDrag : public EditorDrag
{
public:
	CursorDrag (Editor&, EditorCursor&, bool);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return true;
	}

private:
	void fake_locate (samplepos_t);

	EditorCursor& _cursor;
	bool _stop; ///< true to stop the transport on starting the drag, otherwise false
	double _grab_zoom; ///< editor samples per unit when our grab started

	//used for zooming
	int _last_mx;
	int _last_my;
	int _last_dx;
	int _last_y_delta;
};

/** Region fade-in drag */
class FadeInDrag : public RegionDrag
{
public:
	FadeInDrag (Editor&, ArdourCanvas::Item *, RegionView *, std::list<RegionView*> const &, Temporal::TimeDomain);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool y_movement_matters () const {
		return false;
	}

	bool allow_vertical_autoscroll () const {
		return false;
	}

	void setup_pointer_offset ();
};

/** Region fade-out drag */
class FadeOutDrag : public RegionDrag
{
public:
	FadeOutDrag (Editor&, ArdourCanvas::Item *, RegionView *, std::list<RegionView*> const &, Temporal::TimeDomain td);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool y_movement_matters () const {
		return false;
	}

	bool allow_vertical_autoscroll () const {
		return false;
	}

	void setup_pointer_offset ();
};

/** Marker drag */
class MarkerDrag : public EditorDrag
{
public:
	MarkerDrag (Editor&, ArdourCanvas::Item *);
	~MarkerDrag ();

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return false;
	}

	void setup_pointer_offset ();
	void setup_video_offset ();

private:
	void update_item (ARDOUR::Location *);

	ArdourMarker* _marker; ///< marker being dragged
	bool _selection_changed;
	struct CopiedLocationMarkerInfo {
		ARDOUR::Location* location;
		std::vector<ArdourMarker*> markers;
		bool    move_both;
		CopiedLocationMarkerInfo (ARDOUR::Location* l, ArdourMarker* m);
	};

	std::vector<ARDOUR::Location::ChangeSuspender> lcs;

	typedef std::list<CopiedLocationMarkerInfo> CopiedLocationInfo;
	CopiedLocationInfo _copied_locations;
	ArdourCanvas::Points _points;
};

/** Control point drag */
class ControlPointDrag : public EditorDrag
{
public:
	ControlPointDrag (Editor&, ArdourCanvas::Item *);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool active (Editing::MouseMode m);

	bool allow_vertical_autoscroll () const {
		return false;
	}


private:

	ControlPoint* _point;
	double _fixed_grab_x;
	double _fixed_grab_y;
	double _cumulative_y_drag;
	bool     _pushing;
        uint32_t _final_index;
	static double _zero_gain_fraction;

	Temporal::timecnt_t total_dt (GdkEvent*) const;
};

/** Gain or automation line drag */
class LineDrag : public EditorDrag
{
public:
	LineDrag (Editor&e, ArdourCanvas::Item *i);
	~LineDrag ();

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

private:

	AutomationLine* _line;
	double _fixed_grab_x;
	double _fixed_grab_y;
	double _cumulative_y_drag;
	uint32_t _before;
	uint32_t _after;
	bool    have_command;
};

/** Transient feature line drags*/
class FeatureLineDrag : public Drag
{
public:
	FeatureLineDrag (Editor&e, ArdourCanvas::Item *i);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

private:

	ArdourCanvas::Line* _line;
	AudioRegionView* _arv;

	double _region_view_grab_x;
	double _cumulative_x_drag;

	float _before;
	uint32_t _max_x;
};

/** Dragging of a rubberband rectangle for selecting things */
class RubberbandSelectDrag : public Drag
{
public:
	RubberbandSelectDrag (EditingContext&, ArdourCanvas::Item *);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	std::pair<Temporal::timecnt_t,int> move_threshold () const {
		return std::make_pair (Temporal::timecnt_t (8, Temporal::AudioTime), 1);
	}

	void do_select_things (GdkEvent *, bool);

	/** Select some things within a rectangle.
	 *  @param button_state The button state from the GdkEvent.
	 *  @param x1 The left-hand side of the rectangle as a timepos_t
	 *  @param x2 The right-hand side of the rectangle as a timepos_t.
	 *  @param y1 The top of the rectangle in trackview coordinates.
	 *  @param y2 The bottom of the rectangle in trackview coordinates.
	 *  @param drag_in_progress true if the drag is currently happening.
	 */
	virtual void select_things (int button_state, Temporal::timepos_t const & x1, Temporal::timepos_t const & x2, double y1, double y2, bool drag_in_progress) = 0;

	virtual void deselect_things () = 0;

  protected:
	bool _vertical_only;
};

/** A general editor RubberbandSelectDrag (for regions, automation points etc.) */
class EditorRubberbandSelectDrag : public RubberbandSelectDrag
{
public:
	EditorRubberbandSelectDrag (Editor&, ArdourCanvas::Item *);

	void select_things (int, Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, bool);
	void deselect_things ();
  private:
	Editor& editor;
};

/** A RubberbandSelectDrag for selecting MIDI notes */
class MidiRubberbandSelectDrag : public RubberbandSelectDrag
{
public:
	MidiRubberbandSelectDrag (EditingContext&, MidiView *);

	void select_things (int, Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, bool);
	void deselect_things ();

private:
	MidiView* _midi_view;
};

/** A RubberbandSelectDrag for selecting MIDI notes but with no horizontal component */
class MidiVerticalSelectDrag : public RubberbandSelectDrag
{
public:
	MidiVerticalSelectDrag (EditingContext&, MidiView *);

	void select_things (int, Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, bool);
	void deselect_things ();

private:
	MidiView* _midi_view;
};

/** Region drag in time-FX mode */
class TimeFXDrag : public RegionDrag
{
public:
	TimeFXDrag (Editor&, ArdourCanvas::Item *, RegionView *, std::list<RegionView*> const &, Temporal::TimeDomain td);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);
private:
	bool _dragging_start;
};

/** Drag in range select mode */
class SelectionDrag : public EditorDrag
{
public:
	enum Operation {
		CreateSelection,
		SelectionStartTrim,
		SelectionEndTrim,
		SelectionMove,
		SelectionExtend
	};

	SelectionDrag (Editor&, ArdourCanvas::Item *, Operation);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	void setup_pointer_offset ();

private:
	Operation _operation;
	bool _add;
	TrackSelection _track_selection_at_start;
	bool _time_selection_at_start;
	Temporal::timepos_t start_at_start;
	Temporal::timepos_t end_at_start;
};

/** Drag time-selection markers */
class SelectionMarkerDrag : public EditorDrag
{
public:
	SelectionMarkerDrag (Editor&, ArdourCanvas::Item*);

	void start_grab (GdkEvent*, Gdk::Cursor* c = 0);
	void motion (GdkEvent*, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

private:
	bool                _edit_start;
	Temporal::timepos_t _start_at_start;
	Temporal::timepos_t _end_at_start;
};

/** Range marker drag */
class RangeMarkerBarDrag : public EditorDrag
{
public:
	enum Operation {
		CreateSkipMarker,
		CreateRangeMarker,
		CreateTransportMarker,
		CreateCDMarker
	};

	RangeMarkerBarDrag (Editor&, ArdourCanvas::Item *, Operation);
	~RangeMarkerBarDrag ();

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return false;
	}

private:
	void update_item (ARDOUR::Location *);

	Operation _operation;
	ArdourCanvas::Rectangle* _drag_rect;
	bool _copy;
};

/** Drag of rectangle to set zoom */
class MouseZoomDrag : public EditorDrag
{
public:
	MouseZoomDrag (Editor&, ArdourCanvas::Item *);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	std::pair<Temporal::timecnt_t,int> move_threshold () const {
		return std::make_pair (Temporal::timecnt_t (4, Temporal::AudioTime), 4);
	}

private:
	bool _zoom_out;
};

/** Drag of a range of automation data (either on an automation track or region gain),
 *  changing value but not position.
 */
class AutomationRangeDrag : public Drag
{
public:
	AutomationRangeDrag (EditingContext&, AutomationTimeAxisView *, float initial_value, std::list<ARDOUR::TimelineRange> const &);
	AutomationRangeDrag (EditingContext&, std::list<RegionView*> const &, std::list<ARDOUR::TimelineRange> const &, double y_origin, double y_height);

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool x_movement_matters () const {
		return false;
	}

private:
	void setup (std::list<std::shared_ptr<AutomationLine> > const &);
	double y_fraction (double global_y_position) const;
	double value (std::shared_ptr<ARDOUR::AutomationList> list, Temporal::timepos_t const & x) const;

	std::list<ARDOUR::TimelineRange> _ranges;

	/** A line that is part of the drag */
	struct Line {
		std::shared_ptr<AutomationLine> line; ///< the line
		std::list<ControlPoint*> points; ///< points to drag on the line
		std::pair<Temporal::timepos_t, Temporal::timepos_t> range; ///< the range of all points on the line, in session time
		XMLNode* state; ///< the XML state node before the drag
	};

	std::list<Line> _lines;
	double          _y_origin;
	double          _y_height;
	bool            _nothing_to_drag;
	bool            _integral;
	float_t			_initial_value;
};

/** Drag of one edge of an xfade
 */
class CrossfadeEdgeDrag : public Drag
{
public:
	CrossfadeEdgeDrag (Editor&, AudioRegionView*, ArdourCanvas::Item*, bool start);

	void start_grab (GdkEvent*, Gdk::Cursor* c = 0);
	void motion (GdkEvent*, bool);
	void finished (GdkEvent*, bool);
	void aborted (bool);

	bool y_movement_matters () const {
		return false;
	}

	std::pair<Temporal::timecnt_t,int> move_threshold () const {
		return std::make_pair (Temporal::timecnt_t (4, Temporal::AudioTime), 4);
	}

private:
	AudioRegionView* arv;
	bool start;
};

class RegionMarkerDrag : public Drag
{
  public:
	RegionMarkerDrag (Editor&, RegionView*, ArdourCanvas::Item*);
	~RegionMarkerDrag ();

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool y_movement_matters () const {
		return false;
	}

	void setup_pointer_offset ();

  private:
	RegionView* rv;
	ArdourMarker* view;
	ARDOUR::CueMarker model;
	ARDOUR::CueMarker dragging_model;
};


class LollipopDrag : public Drag
{
  public:
	LollipopDrag (EditingContext&, ArdourCanvas::Item*);
	~LollipopDrag ();

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void motion (GdkEvent *, bool);
	void finished (GdkEvent *, bool);
	void aborted (bool);

	bool allow_vertical_autoscroll () const {
		return false;
	}

	bool allow_horizontal_autoscroll () const {
		return false;
	}

	bool x_movement_matters () const {
		return false;
	}

	void setup_pointer_offset ();

  private:
	VelocityGhostRegion*      _region;
	ArdourCanvas::Lollipop*   _primary;
};

template<typename OrderedPointList, typename OrderedPoint>
class FreehandLineDrag : public Drag
{
  public:
	FreehandLineDrag (EditingContext&, ArdourCanvas::Item*, ArdourCanvas::Rectangle&, Temporal::TimeDomain);
	~FreehandLineDrag ();

	void motion (GdkEvent*, bool);
	void finished (GdkEvent*, bool);
	bool mid_drag_key_event (GdkEventKey*);
	virtual void point_added  (ArdourCanvas::Duple const & d, ArdourCanvas::Rectangle const & r, double last_x) {}
	virtual void line_extended  (ArdourCanvas::Duple const & from, ArdourCanvas::Duple const & to, ArdourCanvas::Rectangle const & r, double last_x) {
		point_added (to, r, last_x);
	}

  protected:
	ArdourCanvas::Item* parent; /* we do not own this. If null, use base_rect as the parent */
	ArdourCanvas::Rectangle& base_rect; /* we do not own this */
	ArdourCanvas::PolyLine* dragging_line;
	int direction;
	int edge_x;
	bool did_snap;
	bool line_break_pending;
	OrderedPointList drawn_points;
	ArdourCanvas::Coord line_start_x;
	ArdourCanvas::Coord line_start_y;

	void maybe_add_point (GdkEvent*, Temporal::timepos_t const &, bool first_move);
};

class AutomationDrawDrag : public FreehandLineDrag<Evoral::ControlList::OrderedPoints, Evoral::ControlList::OrderedPoint>
{
  public:
	AutomationDrawDrag (EditingContext&, ArdourCanvas::Item*, ArdourCanvas::Rectangle&, Temporal::TimeDomain);
	~AutomationDrawDrag ();

	void finished (GdkEvent*, bool);
	void aborted (bool) {}
};

class VelocityLineDrag : public FreehandLineDrag<Evoral::ControlList::OrderedPoints, Evoral::ControlList::OrderedPoint>
{
  public:
	VelocityLineDrag (EditingContext&, ArdourCanvas::Rectangle&, Temporal::TimeDomain);
	~VelocityLineDrag ();

	void start_grab (GdkEvent *, Gdk::Cursor* c = 0);
	void finished (GdkEvent*, bool);
	void aborted (bool);
	void point_added  (ArdourCanvas::Duple const & d, ArdourCanvas::Rectangle const & r, double last_x);
	void line_extended (ArdourCanvas::Duple const & from, ArdourCanvas::Duple const & to, ArdourCanvas::Rectangle const & r, double last_x);

 private:
	VelocityGhostRegion* grv;
	bool drag_did_change;
};



#endif /* __gtk2_ardour_editor_drag_h_ */
