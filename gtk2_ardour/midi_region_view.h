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

#ifndef __gtk_ardour_midi_region_view_h__
#define __gtk_ardour_midi_region_view_h__

#include <string>
#include <vector>
#include <stdint.h>

#include "pbd/signals.h"

#include "ardour/midi_model.h"
#include "ardour/types.h"

#include "editing.h"
#include "region_view.h"
#include "midi_time_axis.h"
#include "time_axis_view_item.h"
#include "automation_line.h"
#include "enums.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class Filter;
};

namespace MIDI {
	namespace Name {
		struct PatchPrimaryKey;
	};
};

class SysEx;
class NoteBase;
class Note;
class Hit;
class MidiTimeAxisView;
class GhostRegion;
class AutomationTimeAxisView;
class AutomationRegionView;
class MidiCutBuffer;
class MidiListEditor;
class EditNoteDialog;
class PatchChange;
class ItemCounts;
class CursorContext;

class MidiRegionView : public RegionView
{
public:
	typedef Evoral::Note<Temporal::Beats> NoteType;
	typedef Evoral::Sequence<Temporal::Beats>::Notes Notes;

	MidiRegionView (ArdourCanvas::Container*              parent,
	                RouteTimeAxisView&                    tv,
	                boost::shared_ptr<ARDOUR::MidiRegion> r,
	                double                                samples_per_pixel,
	                uint32_t                              basic_color);

	MidiRegionView (ArdourCanvas::Container*              parent,
	                RouteTimeAxisView&                    tv,
	                boost::shared_ptr<ARDOUR::MidiRegion> r,
	                double                                samples_per_pixel,
	                uint32_t                              basic_color,
	                bool                                  recording,
	                Visibility                            visibility);


	MidiRegionView (const MidiRegionView& other);
	MidiRegionView (const MidiRegionView& other, boost::shared_ptr<ARDOUR::MidiRegion>);

	~MidiRegionView ();

	void init (bool wfd);

	const boost::shared_ptr<ARDOUR::MidiRegion> midi_region() const;

	inline MidiTimeAxisView* midi_view() const
	{ return dynamic_cast<MidiTimeAxisView*>(&trackview); }

	inline MidiStreamView* midi_stream_view() const
	{ return midi_view()->midi_view(); }

	void step_add_note (uint8_t channel, uint8_t number, uint8_t velocity,
	                    Temporal::Beats pos, Temporal::Beats len);
	void step_sustain (Temporal::Beats beats);
	void set_height (double);
	void apply_note_range(uint8_t lowest, uint8_t highest, bool force=false);

	inline ARDOUR::ColorMode color_mode() const { return midi_view()->color_mode(); }

	uint32_t get_fill_color() const;
	void color_handler ();

	void show_step_edit_cursor (Temporal::Beats pos);
	void move_step_edit_cursor (Temporal::Beats pos);
	void hide_step_edit_cursor ();
	void set_step_edit_cursor_width (Temporal::Beats beats);

	void redisplay_model();

	GhostRegion* add_ghost (TimeAxisView&);

	NoteBase* add_note(const boost::shared_ptr<NoteType> note, bool visible);
	void resolve_note(uint8_t note_num, Temporal::Beats end_time);

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

	void begin_write();
	void end_write();
	void extend_active_notes();

	void display_model(boost::shared_ptr<ARDOUR::MidiModel> model);

	void start_note_diff_command (std::string name = "midi edit");
	void note_diff_add_change (NoteBase* ev, ARDOUR::MidiModel::NoteDiffCommand::Property, uint8_t val);
	void note_diff_add_change (NoteBase* ev, ARDOUR::MidiModel::NoteDiffCommand::Property, Temporal::Beats val);
	void note_diff_add_note (const boost::shared_ptr<NoteType> note, bool selected, bool show_velocity = false);
	void note_diff_remove_note (NoteBase* ev);

	void apply_diff (bool as_subcommand = false, bool was_copy = false);
	void abort_command();

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
	void   delete_note (boost::shared_ptr<NoteType>);
	size_t selection_size() { return _selection.size(); }
	void   select_all_notes ();
	void   select_range(Temporal::timepos_t const & start, Temporal::timepos_t const & end);
	void   invert_selection ();
	void   extend_selection ();

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
	bool note_in_region_range(const boost::shared_ptr<NoteType> note, bool& visible) const;

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
	void commit_resizing (NoteBase* primary, bool at_front, double delat_x, bool relative, double snap_delta, bool with_snap);
	void abort_resizing ();

	/** Change the channel of the selection.
	 * @param channel - the channel number of the new channel, zero-based
	 */
	void change_channel(uint8_t channel);

	enum MouseState {
		None,
		Pressed,
		SelectTouchDragging,
		SelectRectDragging,
		SelectVerticalDragging,
		AddDragging
	};

	MouseState mouse_state() const { return _mouse_state; }

	struct NoteResizeData {
		Note                     *note;
		ArdourCanvas::Rectangle  *resize_rect;
	};

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
	 * @return the snapped timepos_t coordinate relative to region start
	 */
	Temporal::timepos_t snap_pixel_to_time (double x, bool ensure_snap = false);

	void goto_previous_note (bool add_to_selection);
	void goto_next_note (bool add_to_selection);
	void change_note_lengths (bool, bool, Temporal::Beats beats, bool start, bool end);
	void change_velocities (bool up, bool fine, bool allow_smush, bool all_together);
	void transpose (bool up, bool fine, bool allow_smush);
	void nudge_notes (bool forward, bool fine);
	void channel_edit ();
	void velocity_edit ();

	void show_list_editor ();

	typedef std::set<NoteBase*> Selection;
	Selection selection () const {
		return _selection;
	}

	void selection_as_notelist (Notes& selected, bool allow_all_if_none_selected = false);

	void enable_display (bool);

	void set_channel_selector_scoped_note(NoteBase* note){ _channel_selection_scoped_note = note; }
	NoteBase* channel_selector_scoped_note(){  return _channel_selection_scoped_note; }

	void trim_front_starting ();
	void trim_front_ending ();

	/** Add a note to the model, and the view, at a canvas (click) coordinate.
	 * \param t time in samples relative to the position of the region
	 * \param y vertical position in pixels
	 * \param length duration of the note in beats
	 * \param state the keyboard modifier mask for the canvas event (click).
	 * \param shift_snap true alters snap behavior to round down always (false if the gui has already done that).
	 */
	void create_note_at (samplepos_t t, double y, Temporal::Beats length, uint32_t state, bool shift_snap);

	/** An external request to clear the note selection, remove MRV from editor
	 * selection.
	 */
	void clear_selection ();

	ARDOUR::InstrumentInfo& instrument_info() const;

	void note_deleted (NoteBase*);

	void show_verbose_cursor_for_new_note_value(boost::shared_ptr<NoteType> current_note, uint8_t new_note) const;

  protected:
	void region_resized (const PBD::PropertyChange&);

	void set_flags (XMLNode *);
	void store_flags ();

	void reset_width_dependent_items (double pixel_width);

	void parameter_changed (std::string const & p);

  protected:
	friend class Editor;

	void clear_note_selection ();
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

  private:

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
	void play_midi_note (boost::shared_ptr<NoteType> note);
	void start_playing_midi_note (boost::shared_ptr<NoteType> note);
	void start_playing_midi_chord (std::vector<boost::shared_ptr<NoteType> > notes);

	/** Clear the note selection of just this midi region
	 */
	void clear_selection_internal ();

	void clear_events ();

	bool canvas_group_event(GdkEvent* ev);
	bool note_canvas_event(GdkEvent* ev);

	void midi_channel_mode_changed ();
	PBD::ScopedConnection _channel_mode_changed_connection;
	void instrument_settings_changed ();
	PBD::ScopedConnection _instrument_changed_connection;

	void change_note_channel (NoteBase *, int8_t, bool relative=false);
	void change_note_velocity(NoteBase* ev, int8_t vel, bool relative=false);
	void change_note_note(NoteBase* ev, int8_t note, bool relative=false);
	void change_note_time(NoteBase* ev, ARDOUR::MidiModel::TimeType, bool relative=false);
	void change_note_length (NoteBase *, ARDOUR::MidiModel::TimeType);
	void trim_note(NoteBase* ev, ARDOUR::MidiModel::TimeType start_delta,
	               ARDOUR::MidiModel::TimeType end_delta);

	void update_drag_selection (Temporal::timepos_t const & start, Temporal::timepos_t const & end, double y0, double y1, bool extend);
	void update_vertical_drag_selection (double last_y, double y, bool extend);

	void add_to_selection (NoteBase*);
	void remove_from_selection (NoteBase*);

	std::string get_note_name (boost::shared_ptr<NoteType> note, uint8_t note_value) const;

	void show_verbose_cursor (std::string const &, double, double) const;
	void show_verbose_cursor (boost::shared_ptr<NoteType>) const;

	uint8_t get_velocity_for_add (ARDOUR::MidiModel::TimeType time) const;

	uint8_t  _current_range_min;
	uint8_t  _current_range_max;

	typedef boost::unordered_map<boost::shared_ptr<NoteType>, NoteBase*>                             Events;
	typedef boost::unordered_map<ARDOUR::MidiModel::PatchChangePtr, boost::shared_ptr<PatchChange> > PatchChanges;
	typedef boost::unordered_map<ARDOUR::MidiModel::constSysExPtr, boost::shared_ptr<SysEx> >        SysExes;
	typedef std::vector<NoteBase*> CopyDragEvents;

	boost::shared_ptr<ARDOUR::MidiModel> _model;
	Events                               _events;
	CopyDragEvents                       _copy_drag_events;
	PatchChanges                         _patch_changes;
	SysExes                              _sys_exes;
	Note**                               _active_notes;
	ArdourCanvas::Container*             _note_group;
	ARDOUR::MidiModel::NoteDiffCommand*  _note_diff_command;
	NoteBase*                            _ghost_note;
	double                               _last_ghost_x;
	double                               _last_ghost_y;
	ArdourCanvas::Rectangle*             _step_edit_cursor;
	Temporal::Beats                      _step_edit_cursor_width;
	Temporal::Beats                      _step_edit_cursor_position;
	NoteBase*                            _channel_selection_scoped_note;

	MouseState _mouse_state;
	int _pressed_button;

	/** Currently selected NoteBase objects */
	Selection _selection;

	MidiCutBuffer* selection_as_cut_buffer () const;

	/** New notes (created in the current command) which should be selected
	 * when they appear after the command is applied. */
	std::set< boost::shared_ptr<NoteType> > _marked_for_selection;

	/** Notes that should be selected when the model is redisplayed. */
	std::set<Evoral::event_id_t> _pending_note_selection;

	/** New notes (created in the current command) which should have visible velocity
	 * when they appear after the command is applied. */
	std::set< boost::shared_ptr<NoteType> > _marked_for_velocity;

	std::vector<NoteResizeData *> _resize_data;

	/** connection used to connect to model's ContentChanged signal */
	PBD::ScopedConnection content_connection;

	NoteBase* find_canvas_note (boost::shared_ptr<NoteType>);
	NoteBase* find_canvas_note (Evoral::event_id_t id);
	Events::iterator _optimization_iterator;

	boost::shared_ptr<PatchChange> find_canvas_patch_change (ARDOUR::MidiModel::PatchChangePtr p);
	boost::shared_ptr<SysEx> find_canvas_sys_ex (ARDOUR::MidiModel::SysExPtr s);

	void update_note (NoteBase*, bool update_ghost_regions = true);
	void update_sustained (Note *, bool update_ghost_regions = true);
	void update_hit (Hit *, bool update_ghost_regions = true);

	void create_ghost_note (double, double, uint32_t state);
	void update_ghost_note (double, double, uint32_t state);

	MidiListEditor* _list_editor;
	bool _no_sound_notes;

	void snap_changed ();
	PBD::ScopedConnection snap_changed_connection;

	bool motion (GdkEventMotion*);
	bool scroll (GdkEventScroll*);
	bool key_press (GdkEventKey*);
	bool key_release (GdkEventKey*);
	bool button_press (GdkEventButton*);
	bool button_release (GdkEventButton*);
	bool enter_notify (GdkEventCrossing*);
	bool leave_notify (GdkEventCrossing*);

	void drop_down_keys ();
	void maybe_select_by_position (GdkEventButton* ev, double x, double y);
	void get_events (Events& e, Evoral::Sequence<Temporal::Beats>::NoteOperator op, uint8_t val, int chan_mask = 0);

	void display_patch_changes_on_channel (uint8_t, bool);

	void connect_to_diskstream ();
	void data_recorded (boost::weak_ptr<ARDOUR::MidiSource>);

	/** Get grid type as beats, or default to 1 if not snapped to beats. */
	Temporal::Beats get_grid_beats(Temporal::timepos_t const & pos) const;

	void remove_ghost_note ();
	void mouse_mode_changed ();
	void enter_internal (uint32_t state);
	void leave_internal ();
	void hide_verbose_cursor ();

	samplecnt_t _last_display_zoom;

	double    _last_event_x;
	double    _last_event_y;
	bool      _entered;
	NoteBase* _entered_note;

	bool _mouse_changed_selection;

	Gtkmm2ext::Color _patch_change_outline;
	Gtkmm2ext::Color _patch_change_fill;

	Temporal::Beats snap_sample_to_grid_underneath (samplepos_t p, bool shift_snap) const;

	PBD::ScopedConnection _mouse_mode_connection;

	boost::shared_ptr<CursorContext> _press_cursor_ctx;

	ARDOUR::ChannelMode get_channel_mode() const;
	uint16_t get_selected_channels () const;

	inline double contents_height() const { return (_height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2); }
	inline double contents_note_range () const { return (double)(_current_range_max - _current_range_min + 1); }
	inline double note_height() const { return contents_height() / contents_note_range(); }

	double note_to_y (uint8_t note) const;
	uint8_t y_to_note (double y) const;
};


#endif /* __gtk_ardour_midi_region_view_h__ */
