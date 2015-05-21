/*
    Copyright (C) 2001-2011 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
	typedef Evoral::Note<Evoral::Beats> NoteType;
	typedef Evoral::Sequence<Evoral::Beats>::Notes Notes;

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
	                    Evoral::Beats pos, Evoral::Beats len);
	void step_sustain (Evoral::Beats beats);
	void set_height (double);
	void apply_note_range(uint8_t lowest, uint8_t highest, bool force=false);

	inline ARDOUR::ColorMode color_mode() const { return midi_view()->color_mode(); }

	uint32_t get_fill_color() const;
	void color_handler ();

	void show_step_edit_cursor (Evoral::Beats pos);
	void move_step_edit_cursor (Evoral::Beats pos);
	void hide_step_edit_cursor ();
	void set_step_edit_cursor_width (Evoral::Beats beats);

	void redisplay_model();

	GhostRegion* add_ghost (TimeAxisView&);

	NoteBase* add_note(const boost::shared_ptr<NoteType> note, bool visible);
	void resolve_note(uint8_t note_num, Evoral::Beats end_time);

	void cut_copy_clear (Editing::CutCopyOp);
	bool paste (framepos_t pos, const ::Selection& selection, PasteContext& ctx);
	void paste_internal (framepos_t pos, unsigned paste_count, float times, const MidiCutBuffer&);

	void add_canvas_patch_change (ARDOUR::MidiModel::PatchChangePtr patch, const std::string& displaytext, bool);

	/** Look up the given time and channel in the 'automation' and set keys accordingly.
	 * @param time the time of the patch change event
	 * @param channel the MIDI channel of the event
	 * @key a reference to an instance of MIDI::Name::PatchPrimaryKey whose fields will
	 *        will be set according to the result of the lookup
	 */
	void get_patch_key_at (Evoral::Beats time, uint8_t channel, MIDI::Name::PatchPrimaryKey& key) const;

	/** Convert a given PatchChange into a PatchPrimaryKey
	 */
	MIDI::Name::PatchPrimaryKey patch_change_to_patch_key (ARDOUR::MidiModel::PatchChangePtr);

	/** Change old_patch to new_patch.
	 * @param old_patch the canvas patch change which is to be altered
	 * @param new_patch new patch
	 */
	void change_patch_change (PatchChange& old_patch, const MIDI::Name::PatchPrimaryKey& new_patch);
	void change_patch_change (ARDOUR::MidiModel::PatchChangePtr, Evoral::PatchChange<Evoral::Beats> const &);

	void add_patch_change (framecnt_t, Evoral::PatchChange<Evoral::Beats> const &);
	void move_patch_change (PatchChange &, Evoral::Beats);
	void delete_patch_change (PatchChange *);
	void edit_patch_change (PatchChange *);

	void delete_sysex (SysEx*);

	/** Change a patch to the next or previous bank/program.
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
	void note_diff_add_change (NoteBase* ev, ARDOUR::MidiModel::NoteDiffCommand::Property, Evoral::Beats val);
	void note_diff_add_note (const boost::shared_ptr<NoteType> note, bool selected, bool show_velocity = false);
	void note_diff_remove_note (NoteBase* ev);

	void apply_diff (bool as_subcommand = false);
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
	void   select_range(framepos_t start, framepos_t end);
	void   invert_selection ();

	void move_selection(double dx, double dy, double cumulative_dy);
	void note_dropped (NoteBase* ev, ARDOUR::frameoffset_t, int8_t d_note);

	void select_notes (std::list<boost::shared_ptr<NoteType> >);
	void select_matching_notes (uint8_t notenum, uint16_t channel_mask, bool add, bool extend);
	void toggle_matching_notes (uint8_t notenum, uint16_t channel_mask);

	/** Return true iff the note is within the extent of the region.
	 * @param visible will be set to true if the note is within the visible note range, false otherwise.
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

	void update_resizing (NoteBase*, bool, double, bool, double);
	void commit_resizing (NoteBase*, bool, double, bool, double);
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
	 * @param explicitly do not use magnetic snap (required for snap delta calculation)
	 * @return the snapped pixel coordinate relative to region start
	 */
	double snap_to_pixel(double x, bool explicitly = false);

	/** Snap a region relative pixel coordinate to frame units.
	 * @param x a pixel coordinate relative to region start
	 * @param explicitly do not use magnetic snap (required for snap delta calculation)
	 * @return the snapped framepos_t coordinate relative to region start
	 */
	framepos_t snap_pixel_to_sample(double x, bool explicitly = false);

	/** Convert a timestamp in beats into frames (both relative to region position) */
	framepos_t region_beats_to_region_frames(Evoral::Beats beats) const;
	/** Convert a timestamp in beats into absolute frames */
	framepos_t region_beats_to_absolute_frames(Evoral::Beats beats) const {
		return _region->position() + region_beats_to_region_frames (beats);
	}
	/** Convert a timestamp in frames to beats (both relative to region position) */
	Evoral::Beats region_frames_to_region_beats(framepos_t) const;
	double region_frames_to_region_beats_double(framepos_t) const;

	/** Convert a timestamp in beats measured from source start into absolute frames */
	framepos_t source_beats_to_absolute_frames(Evoral::Beats beats) const;
	/** Convert a timestamp in beats measured from source start into region-relative frames */
	framepos_t source_beats_to_region_frames(Evoral::Beats beats) const {
		return source_beats_to_absolute_frames (beats) - _region->position();
	}
	/** Convert a timestamp in absolute frames to beats measured from source start*/
	Evoral::Beats absolute_frames_to_source_beats(framepos_t) const;

	ARDOUR::BeatsFramesConverter const & region_relative_time_converter () const {
		return _region_relative_time_converter;
	}

	ARDOUR::BeatsFramesConverter const & source_relative_time_converter () const {
		return _source_relative_time_converter;
	}

	ARDOUR::DoubleBeatsFramesConverter const & region_relative_time_converter_double () const {
		return _region_relative_time_converter_double;
	}

	void goto_previous_note (bool add_to_selection);
	void goto_next_note (bool add_to_selection);
	void change_note_lengths (bool, bool, Evoral::Beats beats, bool start, bool end);
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
	 * \param t time in frames relative to the position of the region
	 * \param y vertical position in pixels
	 * \param length duration of the note in beats
	 * \param snap_t true to snap t to the grid, otherwise false.
	 */
	void create_note_at (framepos_t t, double y, Evoral::Beats length, bool snap_t);

	void clear_selection (bool signal = true) { clear_selection_except (0, signal); }

        ARDOUR::InstrumentInfo& instrument_info() const;
	
protected:
	void region_resized (const PBD::PropertyChange&);

	void set_flags (XMLNode *);
	void store_flags ();

	void reset_width_dependent_items (double pixel_width);

	void parameter_changed (std::string const & p);

private:

	friend class MidiRubberbandSelectDrag;
	friend class MidiVerticalSelectDrag;

	/** Emitted when the selection has been cleared in one MidiRegionView,
	 *  with the expectation that others will clear their selections in
	 *  sympathy.
	 */
	static PBD::Signal1<void, MidiRegionView*> SelectionCleared;
	PBD::ScopedConnection _selection_cleared_connection;
	void selection_cleared (MidiRegionView *);

	/** this handles the case when the "external" world wants us to clear our internal selections
	 */
	PBD::ScopedConnection _clear_midi_selection_connection;
	void clear_midi_selection () { clear_selection(); }

	friend class EditNoteDialog;

	/** Play the NoteOn event of the given note immediately
	 * and schedule the playback of the corresponding NoteOff event.
	 */
	void play_midi_note (boost::shared_ptr<NoteType> note);
	void start_playing_midi_note (boost::shared_ptr<NoteType> note);
	void start_playing_midi_chord (std::vector<boost::shared_ptr<NoteType> > notes);

	void clear_events (bool with_selection_signal = true);

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

	void clear_selection_except (NoteBase* ev, bool signal = true);
	void update_drag_selection (framepos_t start, framepos_t end, double y0, double y1, bool extend);
	void update_vertical_drag_selection (double last_y, double y, bool extend);

	void add_to_selection (NoteBase*);
	void remove_from_selection (NoteBase*);

	void show_verbose_cursor (std::string const &, double, double) const;
	void show_verbose_cursor (boost::shared_ptr<NoteType>) const;

	uint8_t get_velocity_for_add (ARDOUR::MidiModel::TimeType time) const;

	uint8_t  _current_range_min;
	uint8_t  _current_range_max;
	
	typedef std::list<NoteBase*>                          Events;
	typedef std::vector< boost::shared_ptr<PatchChange> > PatchChanges;
	typedef std::vector< boost::shared_ptr<SysEx> >       SysExes;

	ARDOUR::BeatsFramesConverter _region_relative_time_converter;
	ARDOUR::BeatsFramesConverter _source_relative_time_converter;
	ARDOUR::DoubleBeatsFramesConverter _region_relative_time_converter_double;

	boost::shared_ptr<ARDOUR::MidiModel> _model;
	Events                               _events;
	PatchChanges                         _patch_changes;
	SysExes                              _sys_exes;
	Note**                               _active_notes;
	ArdourCanvas::Container*             _note_group;
	ARDOUR::MidiModel::NoteDiffCommand*  _note_diff_command;
	NoteBase*                            _ghost_note;
	double                               _last_ghost_x;
	double                               _last_ghost_y;
	ArdourCanvas::Rectangle*             _step_edit_cursor;
	Evoral::Beats                        _step_edit_cursor_width;
	Evoral::Beats                        _step_edit_cursor_position;
	NoteBase*                            _channel_selection_scoped_note;

	/** A group used to temporarily reparent _note_group to during start trims, so
	 *  that the notes don't move with the parent region view.
	 */
	ArdourCanvas::Container*                 _temporary_note_group;

	MouseState _mouse_state;
	int _pressed_button;

	/** Currently selected NoteBase objects */
	Selection _selection;

	bool _sort_needed;
	void time_sort_events ();

	MidiCutBuffer* selection_as_cut_buffer () const;

	/** New notes (created in the current command) which should be selected
	 * when they appear after the command is applied. */
	std::set< boost::shared_ptr<NoteType> > _marked_for_selection;

	/** Notes that should be selected when the model is redisplayed. */
	std::set< boost::shared_ptr<NoteType> > _pending_note_selection;

	/** New notes (created in the current command) which should have visible velocity
	 * when they appear after the command is applied. */
	std::set< boost::shared_ptr<NoteType> > _marked_for_velocity;

	std::vector<NoteResizeData *> _resize_data;

	/** connection used to connect to model's ContentChanged signal */
	PBD::ScopedConnection content_connection;

	NoteBase* find_canvas_note (boost::shared_ptr<NoteType>);
	NoteBase* find_canvas_note (NoteType);
	Events::iterator _optimization_iterator;

	void update_note (NoteBase*, bool update_ghost_regions = true);
	void update_sustained (Note *, bool update_ghost_regions = true);
	void update_hit (Hit *, bool update_ghost_regions = true);

	void create_ghost_note (double, double);
	void update_ghost_note (double, double);

	MidiListEditor* _list_editor;
	bool _no_sound_notes;

	PBD::ScopedConnection note_delete_connection;
	void maybe_remove_deleted_note_from_selection (NoteBase*);

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
	void get_events (Events& e, Evoral::Sequence<Evoral::Beats>::NoteOperator op, uint8_t val, int chan_mask = 0);

	void display_patch_changes_on_channel (uint8_t, bool);

	void connect_to_diskstream ();
	void data_recorded (boost::weak_ptr<ARDOUR::MidiSource>);

	/** Get grid type as beats, or default to 1 if not snapped to beats. */
	Evoral::Beats get_grid_beats(framepos_t pos) const;

	void remove_ghost_note ();
	void mouse_mode_changed ();
	void enter_internal ();
	void leave_internal ();

	framecnt_t _last_display_zoom;

	double _last_event_x;
	double _last_event_y;
	bool   _grabbed_keyboard;
	bool   _entered;

	bool _mouse_changed_selection;

	framepos_t snap_frame_to_grid_underneath (framepos_t p, framecnt_t &) const;
	
	PBD::ScopedConnection _mouse_mode_connection;

	boost::shared_ptr<CursorContext> _press_cursor_ctx;

        ARDOUR::ChannelMode get_channel_mode() const;
        uint16_t get_selected_channels () const;
};


#endif /* __gtk_ardour_midi_region_view_h__ */
