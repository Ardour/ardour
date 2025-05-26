/*
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <string>
#include <vector>
#include <stdint.h>

#include <unordered_map>

#include <sigc++/signal.h>

#include "pbd/signals.h"

#include "ardour/midi_model.h"
#include "ardour/types.h"

#include "canvas/rectangle.h"

#include "editing.h"
#include "region_view.h"
#include "midi_view_background.h"
#include "time_axis_view_item.h"
#include "editor_automation_line.h"
#include "enums.h"
#include "line_merger.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Filter;
};

namespace MIDI {
	namespace Name {
		struct PatchPrimaryKey;
	};
};

class SysEx;
class Note;
class Hit;
class MidiTimeAxisView;
class NoteBase;
class GhostRegion;
class AutomationTimeAxisView;
class AutomationRegionView;
class MidiCutBuffer;
class MidiListEditor;
class EditNoteDialog;
class PatchChange;
class ItemCounts;
class CursorContext;
class VelocityGhostRegion;
class EditingContext;
class PasteContext;
class Drag;

class StartBoundaryRect : public ArdourCanvas::Rectangle
{
  public:
	StartBoundaryRect (ArdourCanvas::Item* p) : ArdourCanvas::Rectangle (p) {}

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;
	bool covers (ArdourCanvas::Duple const& point) const;
	void compute_bounding_box () const;
};

class EndBoundaryRect : public ArdourCanvas::Rectangle
{
  public:
	EndBoundaryRect (ArdourCanvas::Item* p) : ArdourCanvas::Rectangle (p) {}

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;
	bool covers (ArdourCanvas::Duple const& point) const;
	void compute_bounding_box () const;
};

class MidiView : public virtual sigc::trackable, public LineMerger
{
  public:
	typedef Evoral::Note<Temporal::Beats> NoteType;
	typedef Evoral::Sequence<Temporal::Beats>::Notes Notes;

	MidiView (std::shared_ptr<ARDOUR::MidiTrack> mt,
	          ArdourCanvas::Item&      parent,
	          EditingContext&          ec,
	          MidiViewBackground&      bg,
	          uint32_t                 basic_color);
	MidiView (MidiView const & other);

	virtual ~MidiView ();

	void init (bool wfd);

	virtual void set_samples_per_pixel (double) {};

	virtual bool display_is_enabled() const { return true; }

	virtual ArdourCanvas::Item* drag_group() const = 0;

	void step_add_note (uint8_t channel, uint8_t number, uint8_t velocity,
	                    Temporal::Beats pos, Temporal::Beats len);
	void step_sustain (Temporal::Beats beats);
	virtual void set_height (double);
	void apply_note_range(uint8_t lowest, uint8_t highest, bool force=false);

	// inline ARDOUR::ColorMode color_mode() const { return _background->color_mode(); }

	virtual uint32_t get_fill_color() const;
	void color_handler ();

	void show_step_edit_cursor (Temporal::Beats pos);
	void move_step_edit_cursor (Temporal::Beats pos);
	void hide_step_edit_cursor ();
	void set_step_edit_cursor_width (Temporal::Beats beats);

	virtual GhostRegion* add_ghost (TimeAxisView&) { return nullptr; }
	virtual std::string get_modifier_name() const;

	virtual void set_region (std::shared_ptr<ARDOUR::MidiRegion>);
	virtual void set_track (std::shared_ptr<ARDOUR::MidiTrack>);
	virtual void set_model (std::shared_ptr<ARDOUR::MidiModel>);

	void set_show_source (bool yn);
	bool show_source () const { return _show_source; }

	NoteBase* add_note(const std::shared_ptr<NoteType> note, bool visible);

	void cut_copy_clear (Editing::CutCopyOp);
	bool paste (Temporal::timepos_t const & pos, const ::Selection& selection, PasteContext& ctx);
	void paste_internal (Temporal::timepos_t const & pos, unsigned paste_count, float times, const MidiCutBuffer&);

	void add_canvas_patch_change (ARDOUR::MidiModel::PatchChangePtr patch);
	void remove_canvas_patch_change (PatchChange* pc);

	/** Look up the given time and channel in the 'automation' and set keys accordingly.
	 * @param time the time of the patch change event
	 * @param channel the MIDI channel of the event
	 * @param key a reference to an instance of MIDI::Name::PatchPrimaryKey whose fields will
	 *        will be set according to the result of the lookup
	 */
	void get_patch_key_at (Temporal::Beats time, uint8_t channel, MIDI::Name::PatchPrimaryKey& key) const;

	/** Convert a given PatchChange into a PatchPrimaryKey
	 */
	MIDI::Name::PatchPrimaryKey patch_change_to_patch_key (ARDOUR::MidiModel::PatchChangePtr);

	/** Change old_patch to new_patch.
	 * @param old_patch the canvas patch change which is to be altered
	 * @param new_patch new patch
	 */
	void change_patch_change (PatchChange& old_patch, const MIDI::Name::PatchPrimaryKey& new_patch);
	void change_patch_change (ARDOUR::MidiModel::PatchChangePtr, Evoral::PatchChange<Temporal::Beats> const &);

	void add_patch_change (Temporal::timecnt_t const &, Evoral::PatchChange<Temporal::Beats> const &);
	void move_patch_change (PatchChange &, Temporal::Beats);
	void delete_patch_change (PatchChange *);
	void edit_patch_change (PatchChange *);

	void delete_sysex (SysEx*);

	/** Change a patch to the next or previous bank/program.
	 *
	 * @param patch The patch-change instance (canvas item)
	 * @param bank If true, step bank, otherwise, step program.
	 * @param delta Amount to adjust number.
	 */
	void step_patch (PatchChange& patch, bool bank, int delta);

	/** Displays all patch change events in the region as flags on the canvas.
	 */
	void display_patch_changes();

	/** Displays all system exclusive events in the region as flags on the canvas.
	 */
	void display_sysexes();

	void begin_write ();
	void end_write ();
	void extend_active_notes ();
	void extend_active_notes (Temporal::timecnt_t const &);

	virtual void begin_drag_edit (std::string const & why);
	void end_drag_edit ();

	void display_model(std::shared_ptr<ARDOUR::MidiModel> model);
	std::shared_ptr<ARDOUR::MidiModel> model() const { return _model; }

	/* note_diff commands should start here; this initiates an undo record */
	void start_note_diff_command (std::string name = "midi edit");

	void note_diff_add_change (NoteBase* ev, ARDOUR::MidiModel::NoteDiffCommand::Property, uint8_t val);
	void note_diff_add_change (NoteBase* ev, ARDOUR::MidiModel::NoteDiffCommand::Property, Temporal::Beats val);
	void note_diff_add_note (const std::shared_ptr<NoteType> note, bool selected, bool show_velocity = false);
	void note_diff_remove_note (NoteBase* ev);

	/* note_diff commands should be completed with one of these calls; they may (or may not) commit the undo record */
	void apply_note_diff (bool as_subcommand = false, bool was_copy = false);
	void abort_note_diff();

	void   note_entered(NoteBase* ev);
	void   note_left(NoteBase* ev);
	void   patch_entered (PatchChange *);
	void   patch_left (PatchChange *);
	void   sysex_entered (SysEx* p);
	void   sysex_left (SysEx* p);
	void   note_mouse_position (float xfraction, float yfraction, bool can_set_cursor=true);
	void   unique_select(NoteBase* ev);
	void   note_selected(NoteBase* ev, bool add, bool extend=false);
	void   note_deselected(NoteBase* ev);
	void   delete_selection();
	void   delete_note (std::shared_ptr<NoteType>);
	size_t selection_size() { return _selection.size(); }
	void   select_all_notes ();
	void   select_range(Temporal::timepos_t const & start, Temporal::timepos_t const & end);
	void   invert_selection ();
	void   extend_selection ();
	void   duplicate_selection ();

	Temporal::Beats earliest_in_selection ();
	void move_selection(Temporal::timecnt_t const & dx, double dy, double cumulative_dy);
	void note_dropped (NoteBase* ev, Temporal::timecnt_t const & d_qn, int8_t d_note, bool copy);
	NoteBase* copy_selection (NoteBase* primary);
	void move_copies(Temporal::timecnt_t const & dx_qn, double dy, double cumulative_dy);

	void select_notes (std::list<Evoral::event_id_t>, bool allow_audition);
	void select_matching_notes (uint8_t notenum, uint16_t channel_mask, bool add, bool extend);
	void toggle_matching_notes (uint8_t notenum, uint16_t channel_mask);

	/** Test if a note is within this region's range
	 *
	 * @param note the note to test
	 * @param visible will be set to true if the note is within the visible note range, false otherwise.
	 * @return true iff the note is within the (time) extent of the region.
	 */
	virtual bool note_in_region_range(const std::shared_ptr<NoteType> note, bool& visible) const;
	/* Test if a note is within this region's time range. Return true if so */
	virtual bool note_in_region_time_range(const std::shared_ptr<NoteType> note) const;

	/** Get the region position in pixels relative to session. */
	double get_position_pixels();

	/** Get the region end position in pixels relative to session. */
	double get_end_position_pixels();

	/** Begin resizing of some notes.
	 * Called by CanvasMidiNote when resizing starts.
	 * @param at_front which end of the note (true == note on, false == note off)
	 */
	void begin_resizing(bool at_front);

	void update_resizing (NoteBase* primary, bool at_front, double delta_x, bool relative, double snap_delta, bool with_snap);
	void finish_resizing (NoteBase* primary, bool at_front, double delat_x, bool relative, double snap_delta, bool with_snap);
	void abort_resizing ();

	struct NoteResizeData {
		::Note                  *note;
		ArdourCanvas::Rectangle *resize_rect;
	};

	/* Convert a position to a distance (origin+position) relative to the
	 * start of this MidiView.
	 *
	 * What this is relative to will depend on whether or not _show_source
	 * is true.
	 */

	Temporal::timecnt_t view_position_to_model_position (Temporal::timepos_t const & p) const;

	Temporal::timepos_t source_beats_to_timeline (Temporal::Beats const &) const;

	Temporal::timepos_t start() const;

	/** Snap a region relative pixel coordinate to pixel units.
	 * @param x a pixel coordinate relative to region start
	 * @param ensure_snap do not use magnetic snap (required for snap delta calculation)
	 * @return the snapped pixel coordinate relative to region start
	 */
	double snap_to_pixel(double x, bool ensure_snap = false);

	/** Snap a region relative pixel coordinate to time units.
	 * @param x a pixel coordinate relative to region start
	 * @param ensure_snap ignore SnapOff and magnetic snap.
	 * Required for inverting snap logic with modifier keys and snap delta calculation.
	 * @return the snapped timecnt_t coordinate relative to region start
	 */
	Temporal::timecnt_t snap_pixel_to_time (double x, bool ensure_snap = false);

	void goto_previous_note (bool add_to_selection);
	void goto_next_note (bool add_to_selection);
	void change_note_lengths (bool, bool, Temporal::Beats beats, bool start, bool end);
	void change_velocities (bool up, bool fine, bool allow_smush, bool all_together);
	void set_velocity (NoteBase* primary, int velocity);
	bool set_velocity_for_notes (std::vector<NoteBase*>& notes, int velocity);
	bool set_velocities_for_notes (std::vector<NoteBase*>& notes, std::vector<int>& velocities);
	void transpose (bool up, bool fine, bool allow_smush);
	void nudge_notes (bool forward, bool fine);
	void channel_edit ();
	void velocity_edit ();

	void show_list_editor ();

	void set_note_range (uint8_t low, uint8_t high);
	void maybe_set_note_range (uint8_t low, uint8_t high);
	virtual void set_visibility_note_range (MidiViewBackground::VisibleNoteRange, bool);

	typedef std::set<NoteBase*> Selection;
	Selection const & selection () const {
		return _selection;
	}

	void selection_as_notelist (Notes& selected, bool allow_all_if_none_selected = false);

	void set_channel_selector_scoped_note(NoteBase* note){ _channel_selection_scoped_note = note; }
	NoteBase* channel_selector_scoped_note(){  return _channel_selection_scoped_note; }

	/** Add a note to the model, and the view, at a canvas (click) coordinate.
	 * \param t time in samples relative to the position of the region
	 * \param y vertical position in pixels
	 * \param length duration of the note in beats
	 * \param state the keyboard modifier mask for the canvas event (click).
	 * \param shift_snap true alters snap behavior to round down always (false if the gui has already done that).
	 */
	void create_note_at (Temporal::timepos_t const & t, double y, Temporal::Beats length, uint32_t state, bool shift_snap);

	/** An external request to clear the note selection, remove MRV from editor
	 * selection.
	 */
	void clear_selection ();

	void note_deleted (NoteBase*);
	void clear_note_selection ();

	void shift_midi (Temporal::timepos_t const &, bool model);

	void show_verbose_cursor_for_new_note_value(std::shared_ptr<NoteType> current_note, uint8_t new_note) const;

	std::shared_ptr<ARDOUR::MidiTrack>  midi_track() const { return _midi_track; }
	std::shared_ptr<ARDOUR::MidiRegion> midi_region() const { return _midi_region; }
	EditingContext& editing_context() const { return _editing_context; }
	MidiViewBackground& midi_context() const { return _midi_context; }

	void clip_data_recorded (samplecnt_t);

	virtual void select_self (bool add) {}
	virtual void unselect_self () {}
	void select_self () { select_self (false); }
	virtual void select_self_uniquely () {}

	void show_start (bool yn);
	void show_end (bool yn);

	virtual bool midi_canvas_group_event(GdkEvent* ev);

	int visible_channel() const { return _visible_channel; }
	void set_visible_channel (int, bool clear_selection = true);
	PBD::Signal<void()> VisibleChannelChanged;

  protected:
	void init (std::shared_ptr<ARDOUR::MidiTrack>);
	virtual void region_resized (const PBD::PropertyChange&);

	void set_flags (XMLNode *);
	void store_flags ();

	virtual void reset_width_dependent_items (double pixel_width);

	void redisplay (bool view_only);
	bool note_editable (NoteBase const *) const;

  protected:
	friend class EditingContext;
	friend class Editor; // grr, C++ does not allow inheritance of friendship

	void invert_note_selection ();
	void extend_note_selection ();

	void move_note_starts_earlier_fine () { change_note_lengths (true, false, Temporal::Beats(), true, false); }
	void move_note_starts_earlier () { change_note_lengths (false, false, Temporal::Beats(), true, false); }
	void move_note_ends_later_fine () { change_note_lengths (true, false, Temporal::Beats(), false, true); }
	void move_note_ends_later () { change_note_lengths (false, false, Temporal::Beats(), false, true); }
	void move_note_starts_later_fine () { change_note_lengths (true, true, Temporal::Beats(), true, false); }
	void move_note_starts_later () { change_note_lengths (false, true, Temporal::Beats(), true, false); }
	void move_note_ends_earlier_fine () { change_note_lengths (true, true, Temporal::Beats(), false, true); }
	void move_note_ends_earlier () { change_note_lengths (false, true, Temporal::Beats(), false, true); }

	void select_next_note () { goto_next_note (false); }
	void select_previous_note () { goto_previous_note (false); }
	void add_select_next_note () { goto_next_note (true); }
	void add_select_previous_note () { goto_previous_note (true); }

	void increase_note_velocity ()                     { change_velocities (true, false, false, false); }
	void increase_note_velocity_fine ()                { change_velocities (true, true, false, false); }
	void increase_note_velocity_smush ()               { change_velocities (true, false, true, false); }
	void increase_note_velocity_together ()            { change_velocities (true, false, false, true); }
	void increase_note_velocity_fine_smush ()          { change_velocities (true, true, true, false); }
	void increase_note_velocity_fine_together ()       { change_velocities (true, true, false, true); }
	void increase_note_velocity_smush_together ()      { change_velocities (true, false, true, true); }
	void increase_note_velocity_fine_smush_together () { change_velocities (true, true, true, true); }

	void decrease_note_velocity ()                     { change_velocities (false, false, false, false); }
	void decrease_note_velocity_fine ()                { change_velocities (false, true, false, false); }
	void decrease_note_velocity_smush ()               { change_velocities (false, false, true, false); }
	void decrease_note_velocity_together ()            { change_velocities (false, false, false, true); }
	void decrease_note_velocity_fine_smush ()          { change_velocities (false, true, true, false); }
	void decrease_note_velocity_fine_together ()       { change_velocities (false, true, false, true); }
	void decrease_note_velocity_smush_together ()      { change_velocities (false, false, true, true); }
	void decrease_note_velocity_fine_smush_together () { change_velocities (false, true, true, true); }

	void transpose_up_octave () { transpose (true, false, false); }
	void transpose_up_octave_smush () { transpose (true, false, true); }
	void transpose_up_tone () { transpose (true, true, false); }
	void transpose_up_tone_smush () { transpose (true, true, true); }

	void transpose_down_octave () { transpose (false, false, false); }
	void transpose_down_octave_smush () { transpose (false, false, true); }
	void transpose_down_tone () { transpose (false, true, false); }
	void transpose_down_tone_smush () { transpose (false, true, true); }

	void nudge_notes_later () { nudge_notes (true, false); }
	void nudge_notes_later_fine () { nudge_notes (true, true); }
	void nudge_notes_earlier () { nudge_notes (false, false); }
	void nudge_notes_earlier_fine () { nudge_notes (false, true); }

	void quantize_selected_notes ();

  protected:
	friend class MidiRubberbandSelectDrag;
	friend class MidiVerticalSelectDrag;
	friend class NoteDrag;
	friend class NoteCreateDrag;
	friend class HitCreateDrag;
	friend class MidiGhostRegion;

	friend class EditNoteDialog;

	/** Play the NoteOn event of the given note immediately
	 * and schedule the playback of the corresponding NoteOff event.
	 */
	void play_midi_note (std::shared_ptr<NoteType> note);
	void start_playing_midi_note (std::shared_ptr<NoteType> note);
	void start_playing_midi_chord (std::vector<std::shared_ptr<NoteType> > notes);

	/** Clear the note selection of just this midi region
	 */
	void clear_selection_internal ();

	void clear_events ();
	virtual void clear_ghost_events() {}
	virtual void ghosts_model_changed() {}
	virtual void ghosts_view_changed() {}
	virtual void ghost_remove_note (NoteBase*) {}
	virtual void ghost_add_note (NoteBase*) {}
	virtual void ghost_sync_selection (NoteBase*) {}

	bool note_canvas_event(GdkEvent* ev);

	PBD::ScopedConnectionList connections_requiring_model;
	PBD::ScopedConnection track_going_away_connection;
	PBD::ScopedConnectionList region_connections;
	void track_going_away ();
	void region_going_away ();

	void midi_channel_mode_changed ();
	void instrument_settings_changed ();

	void change_note_channel (NoteBase *, int8_t, bool relative=false);
	void change_note_velocity(NoteBase* ev, int8_t vel, bool relative=false);
	uint8_t change_note_note(NoteBase* ev, int8_t note, bool relative=false);
	void change_note_time(NoteBase* ev, ARDOUR::MidiModel::TimeType, bool relative=false);
	void change_note_length (NoteBase *, ARDOUR::MidiModel::TimeType);
	void trim_note(NoteBase* ev, ARDOUR::MidiModel::TimeType start_delta,
	               ARDOUR::MidiModel::TimeType end_delta);

	void update_drag_selection (Temporal::timepos_t const & start, Temporal::timepos_t const & end, double y0, double y1, bool extend);
	void update_vertical_drag_selection (double last_y, double y, bool extend);

	void add_to_selection (NoteBase*);
	void remove_from_selection (NoteBase*);

	std::string get_note_name (std::shared_ptr<NoteType> note, uint8_t note_value) const;

	void show_verbose_cursor (std::string const &, double, double) const;
	void show_verbose_cursor (std::shared_ptr<NoteType>) const;

	uint8_t get_velocity_for_add (ARDOUR::MidiModel::TimeType time) const;
	uint8_t get_channel_for_add (ARDOUR::MidiModel::TimeType time) const;

	typedef std::unordered_map<std::shared_ptr<NoteType>, NoteBase*>                             Events;
	typedef std::unordered_map<ARDOUR::MidiModel::PatchChangePtr, std::shared_ptr<PatchChange> > PatchChanges;
	typedef std::unordered_map<ARDOUR::MidiModel::constSysExPtr, std::shared_ptr<SysEx> >        SysExes;
	typedef std::vector<NoteBase*> CopyDragEvents;

	std::shared_ptr<ARDOUR::MidiTrack>   _midi_track;
	EditingContext&                      _editing_context;
	MidiViewBackground&                  _midi_context;
	std::shared_ptr<ARDOUR::MidiModel>   _model;
	std::shared_ptr<ARDOUR::MidiRegion>  _midi_region;
	Events                               _events;
	CopyDragEvents                       _copy_drag_events;
	PatchChanges                         _patch_changes;
	SysExes                              _sys_exes;
	Note**                               _active_notes;
	Temporal::timecnt_t                   active_note_end;
	ArdourCanvas::Container*             _note_group;
	ARDOUR::MidiModel::NoteDiffCommand*  _note_diff_command;
	NoteBase*                            _ghost_note;
	double                               _last_ghost_x;
	double                               _last_ghost_y;
	ArdourCanvas::Rectangle*             _step_edit_cursor;
	Temporal::Beats                      _step_edit_cursor_width;
	Temporal::Beats                      _step_edit_cursor_position;
	NoteBase*                            _channel_selection_scoped_note;
	StartBoundaryRect*                   _start_boundary_rect;
	EndBoundaryRect*                     _end_boundary_rect;
	bool                                 _show_source;
	Drag*                                 selection_drag;
	Drag*                                 draw_drag;
	int                                  _visible_channel;

	/** Currently selected NoteBase objects */
	Selection _selection;

	MidiCutBuffer* selection_as_cut_buffer () const;

	/** New notes (created in the current command) which should be selected
	 * when they appear after the command is applied. */
	std::set< std::shared_ptr<NoteType> > _marked_for_selection;

	/** Notes that should be selected when the model is redisplayed. */
	std::set<Evoral::event_id_t> _pending_note_selection;

	/** New notes (created in the current command) which should have visible velocity
	 * when they appear after the command is applied. */
	std::set< std::shared_ptr<NoteType> > _marked_for_velocity;

	std::vector<NoteResizeData *> _resize_data;

	/** connection used to connect to model's ContentChanged signal */

	NoteBase* find_canvas_note (std::shared_ptr<NoteType>);
	NoteBase* find_canvas_note (Evoral::event_id_t id);
	Events::iterator _optimization_iterator;

	std::shared_ptr<PatchChange> find_canvas_patch_change (ARDOUR::MidiModel::PatchChangePtr p);
	std::shared_ptr<SysEx> find_canvas_sys_ex (ARDOUR::MidiModel::SysExPtr s);

	friend class VelocityDisplay;
	void sync_velocity_drag (double factor);

	void update_note (NoteBase*);
	virtual void update_sustained (Note *);
	virtual void update_hit (Hit *);

	void create_ghost_note (double, double, uint32_t state);
	void update_ghost_note (double, double, uint32_t state);

	MidiListEditor* _list_editor;
	bool _no_sound_notes;

	void snap_changed ();

	virtual bool motion (GdkEventMotion*);
	virtual bool scroll (GdkEventScroll*);
	virtual bool key_press (GdkEventKey*);
	virtual bool key_release (GdkEventKey*);
	virtual bool button_press (GdkEventButton*);
	virtual bool button_release (GdkEventButton*);
	virtual bool enter_notify (GdkEventCrossing*);
	virtual bool leave_notify (GdkEventCrossing*);

	void drop_down_keys ();
	void maybe_select_by_position (GdkEventButton* ev, double x, double y);
	void get_events (Events& e, Evoral::Sequence<Temporal::Beats>::NoteOperator op, uint8_t val, int chan_mask = 0);

	void display_patch_changes_on_channel (uint8_t, bool);

	void data_recorded (std::weak_ptr<ARDOUR::MidiSource>);

	/** Get grid type as beats, or default to 1 if not snapped to beats. */
	Temporal::Beats get_grid_beats(Temporal::timepos_t const & pos) const;

	Temporal::Beats get_draw_length_beats(Temporal::timepos_t const & pos) const;

	void remove_ghost_note ();
	virtual void mouse_mode_changed ();
	virtual void enter_internal (uint32_t state);
	virtual void leave_internal ();
	void hide_verbose_cursor ();

	samplecnt_t _last_display_zoom;

	double    _last_event_x;
	double    _last_event_y;
	bool      _entered;
	NoteBase* _entered_note;
	bool      _select_all_notes_after_add;

	bool _mouse_changed_selection;

	Gtkmm2ext::Color _patch_change_outline;
	Gtkmm2ext::Color _patch_change_fill;

	ARDOUR::ChannelMode get_channel_mode() const;
	uint16_t get_selected_channels () const;

	virtual double height() const;

	virtual double contents_height() const { return height() - 2; }
	inline double note_height() const { return contents_height() / _midi_context.contents_note_range(); }

	double note_to_y (uint8_t note) const { return _midi_context.note_to_y (note); }
	uint8_t y_to_note (double y) const { return _midi_context.y_to_note (y); }

	void update_patch_changes ();
	void update_sysexes ();
	void view_changed ();
	void model_changed ();
	void note_mode_changed ();

	void sync_ghost_selection (NoteBase*);

	struct SplitInfo {
		Temporal::Beats time;
		Temporal::Beats base_len;
		int             note;
		int             channel;
		int             velocity;
		int             off_velocity;

		SplitInfo (Temporal::Beats const & t, Temporal::Beats const & l, int n, int c, int v, int ov)
			: time (t)
			, base_len (l)
			, note (n)
			, channel (c)
			, velocity (v)
			, off_velocity (ov) {}
	};
	std::vector<SplitInfo> split_info;
	bool in_note_split;

	uint32_t split_tuple;
	bool     note_splitting;
	bool    _extensible; /* if true, we can add data beyond the current region/source end */

	bool extensible() const { return _extensible; }
	void set_extensible (bool yn) { _extensible = yn; }

	void start_note_splitting ();
	void end_note_splitting ();

	void split_notes_grid ();
	void split_notes_more ();
	void split_notes_less ();
	void join_notes ();
	void join_notes_on_channel (int channel);

	void add_split_notes ();
	void region_update_sustained (Note *, double&, double&, double&, double&);
	void clip_capture_update_sustained (Note *, double&, double&, double&, double&);

	void size_start_rect ();
	void size_end_rect ();
	bool start_boundary_event (GdkEvent*);
	bool end_boundary_event (GdkEvent*);

	virtual void add_control_points_to_selection (Temporal::timepos_t const &, Temporal::timepos_t const &, double y0, double y1) {}

	void color_note (NoteBase*, int channel);
};


