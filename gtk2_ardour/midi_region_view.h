/*
    Copyright (C) 2001-2007 Paul Davis

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

#include <vector>

#include <libgnomecanvasmm.h>
#include <libgnomecanvasmm/polygon.h>
#include <ardour/midi_track.h>
#include <ardour/midi_region.h>
#include <ardour/midi_model.h>
#include <ardour/diskstream.h>
#include <ardour/types.h>

#include "region_view.h"
#include "midi_time_axis.h"
#include "time_axis_view_item.h"
#include "automation_line.h"
#include "enums.h"
#include "canvas.h"
#include "canvas-note.h"
#include "canvas-note-event.h"
#include "canvas-program-change.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
};

namespace MIDI {
	namespace Name {
		struct PatchPrimaryKey;
	};
};

class MidiTimeAxisView;
class GhostRegion;
class AutomationTimeAxisView;
class AutomationRegionView;

class MidiRegionView : public RegionView
{
  public:
	typedef Evoral::Note<ARDOUR::MidiModel::TimeType> NoteType;

	MidiRegionView (ArdourCanvas::Group *,
	                RouteTimeAxisView&,
	                boost::shared_ptr<ARDOUR::MidiRegion>,
	                double initial_samples_per_unit,
	                Gdk::Color& basic_color);
	
	MidiRegionView (const MidiRegionView& other);
	MidiRegionView (const MidiRegionView& other, boost::shared_ptr<ARDOUR::MidiRegion>);

	~MidiRegionView ();

	virtual void init (Gdk::Color& basic_color, bool wfd);

	inline const boost::shared_ptr<ARDOUR::MidiRegion> midi_region() const
		{ return boost::dynamic_pointer_cast<ARDOUR::MidiRegion>(_region); }

	inline MidiTimeAxisView* midi_view() const
		{ return dynamic_cast<MidiTimeAxisView*>(&trackview); }

	inline MidiStreamView* midi_stream_view() const
		{ return midi_view()->midi_view(); }

	void set_height (double);
	void apply_note_range(uint8_t lowest, uint8_t highest, bool force=false);

	inline ARDOUR::ColorMode color_mode() const { return midi_view()->color_mode(); }
	
	void set_frame_color();

	void redisplay_model();

	GhostRegion* add_ghost (TimeAxisView&);

	void add_note(const boost::shared_ptr<NoteType> note);
	void resolve_note(uint8_t note_num, double end_time);
	
	struct ControlEvent
	{
		nframes_t time;
		uint8_t   value;
		uint8_t   channel;
		
		ControlEvent(nframes_t a_time, uint8_t a_value, uint8_t a_channel) 
			: time(a_time), value(a_value), channel(a_channel) {}
		
		ControlEvent& operator=(const ControlEvent& other) {
			time = other.time;
			value = other.value;
			channel = other.channel;
			return *this;
		}
	};
	
	/** Add a new program change flag to the canvas.
	 * @param program the MidiRegionView::ControlEvent to add
	 * @param the text to display in the flag
	 */
	void add_pgm_change(ControlEvent& program, string displaytext);
	
	/** Look up the given time and channel in the 'automation' and set keys accordingly.
	 * @param time the time of the program change event
	 * @param channel the MIDI channel of the event
	 * @key a reference to an instance of MIDI::Name::PatchPrimaryKey whose fields will 
	 *        will be set according to the result of the lookup
	 */
	void get_patch_key_at(double time, uint8_t channel, MIDI::Name::PatchPrimaryKey& key);
	
	/** Change the 'automation' data of old_program to new values which correspond to new_patch.
	 * @param old_program identifies the program change event which is to be altered
	 * @param new_patch defines the new lsb, msb and program number which are to be set in the automation list data
	 */
	void alter_program_change(ControlEvent& old_program, const MIDI::Name::PatchPrimaryKey& new_patch);
	
	/** Alter a given program to the new given one.
	 * (Called on context menu select on CanvasProgramChange)
	 */
	void program_selected(
		ArdourCanvas::CanvasProgramChange& program, 
		const MIDI::Name::PatchPrimaryKey& new_patch);
	
	/** Alter a given program to be its predecessor in the MIDNAM file.
	 */
	void previous_program(ArdourCanvas::CanvasProgramChange& program);

	/** Alters a given program to be its successor in the MIDNAM file.
	 */
	void next_program(ArdourCanvas::CanvasProgramChange& program);
	
	/** Displays all program changed events in the region as flags on the canvas.
	 */
	void display_program_change_flags();

	void begin_write();
	void end_write();
	void extend_active_notes();

	void create_note_at(double x, double y, double length);

	void display_model(boost::shared_ptr<ARDOUR::MidiModel> model);

	void start_delta_command(string name = "midi edit");
	void command_add_note(const boost::shared_ptr<NoteType> note, bool selected);
	void command_remove_note(ArdourCanvas::CanvasNoteEvent* ev);

	void apply_command();
	void abort_command();

	void   note_entered(ArdourCanvas::CanvasNoteEvent* ev);
	void   unique_select(ArdourCanvas::CanvasNoteEvent* ev);
	void   note_selected(ArdourCanvas::CanvasNoteEvent* ev, bool add);
	void   note_deselected(ArdourCanvas::CanvasNoteEvent* ev, bool add);
	void   delete_selection();
	size_t selection_size() { return _selection.size(); }

	void move_selection(double dx, double dy);
	void note_dropped(ArdourCanvas::CanvasNoteEvent* ev, double d_frames, uint8_t d_note);

	/** Get the region position in pixels relative to session. */
	double get_position_pixels();

	/** Begin resizing of some notes.
	 * Called by CanvasMidiNote when resizing starts.
	 * @param note_end which end of the note, NOTE_ON or NOTE_OFF
	 */
	void begin_resizing(ArdourCanvas::CanvasNote::NoteEnd note_end);

	/** Update resizing notes while user drags.
	 * @param note_end which end of the note, NOTE_ON or NOTE_OFF
	 * @param x the difference in mouse motion, ie the motion difference if relative=true
	 *           or the absolute mouse position (track-relative) if relative is false
	 * @param relative true if relative resizing is taking place, false if absolute resizing
	 */
	void update_resizing(ArdourCanvas::CanvasNote::NoteEnd note_end, double x, bool relative);

	/** Finish resizing notes when the user releases the mouse button.
	 * @param note_end which end of the note, NOTE_ON or NOTE_OFF
	 * @param event_x the absolute mouse position (track-relative)
	 * @param relative true if relative resizing is taking place, false if absolute resizing
	 */
	void commit_resizing(ArdourCanvas::CanvasNote::NoteEnd note_end, double event_x, bool relative);

	/** Adjust the velocity on a note, and the selection if applicable.
	 * @param velocity the relative or absolute velocity
	 * @param relative whether velocity is relative or absolute
	 */
	void change_velocity(ArdourCanvas::CanvasNoteEvent* ev, int8_t velocity, bool relative=false);
	
	/** Change the channel of the selection.
	 * @param channel - the channel number of the new channel, zero-based
	 */
	void change_channel(uint8_t channel);

	enum MouseState {
		None,
		Pressed,
		SelectTouchDragging,
		SelectRectDragging,
		AddDragging,
		EraseTouchDragging
	};

	MouseState mouse_state() const { return _mouse_state; }

	struct NoteResizeData {
		ArdourCanvas::CanvasNote  *canvas_note;
		ArdourCanvas::SimpleRect  *resize_rect;
		double                     current_x;
	};
	
	/** Snap a region relative pixel coordinate to pixel units.
	 * for pixel units (double) instead of nframes64_t
	 * @param x a pixel coordinate relative to region start
	 * @return the snapped pixel coordinate relative to region start
	 */
	double snap_to_pixel(double x);

	/** Snap a region relative pixel coordinate to frame units.
	 * @param x a pixel coordinate relative to region start
	 * @return the snapped nframes64_t coordinate relative to region start
	 */
	nframes64_t snap_to_frame(double x);

	/** Snap a region relative frame coordinate to frame units.
	 * @param x a pixel coordinate relative to region start
	 * @return the snapped nframes64_t coordinate relative to region start
	 */
	nframes64_t snap_to_frame(nframes64_t x);
	
	/** Convert a timestamp in beats to frames (both relative to region start) */
	nframes64_t beats_to_frames(double beats) const;
	
	/** Convert a timestamp in frames to beats (both relative to region start) */
	double frames_to_beats(nframes64_t beats) const;
	
  protected:
    /** Allows derived types to specify their visibility requirements
     * to the TimeAxisViewItem parent class.
     */
    MidiRegionView (ArdourCanvas::Group *,
	                RouteTimeAxisView&,
	                boost::shared_ptr<ARDOUR::MidiRegion>,
	                double samples_per_unit,
	                Gdk::Color& basic_color,
	                TimeAxisViewItem::Visibility);

    void region_resized (ARDOUR::Change);

    void set_flags (XMLNode *);
    void store_flags ();

	void reset_width_dependent_items (double pixel_width);

  private:
	/** Play the NoteOn event of the given note immediately
	 * and schedule the playback of the corresponding NoteOff event.
	 */
	void play_midi_note(boost::shared_ptr<NoteType> note);
	
	/** Play the NoteOff-Event of the given note immediately
	 * (scheduled by @ref play_midi_note()).
	 */
	bool play_midi_note_off(boost::shared_ptr<NoteType> note);

	void clear_events();
	void switch_source(boost::shared_ptr<ARDOUR::Source> src);

	bool canvas_event(GdkEvent* ev);
	bool note_canvas_event(GdkEvent* ev);
	
	void midi_channel_mode_changed(ARDOUR::ChannelMode mode, uint16_t mask);
	void midi_patch_settings_changed(std::string model, std::string custom_device_mode);
	
	void change_note_velocity(ArdourCanvas::CanvasNoteEvent* ev, int8_t vel, bool relative=false);

	void clear_selection_except(ArdourCanvas::CanvasNoteEvent* ev);
	void clear_selection() { clear_selection_except(NULL); }
	void update_drag_selection(double last_x, double x, double last_y, double y);

	int8_t   _force_channel;
	uint16_t _last_channel_selection;
	double   _default_note_length;
	uint8_t  _current_range_min;
	uint8_t  _current_range_max;
	
	/// MIDNAM information of the current track: Model name of MIDNAM file
	string   _model_name;
	
	/// MIDNAM information of the current track: CustomDeviceMode
	string   _custom_device_mode;   

	typedef std::vector<ArdourCanvas::CanvasNoteEvent*> Events;
	typedef std::vector< boost::shared_ptr<ArdourCanvas::CanvasProgramChange> > PgmChanges;
	
	boost::shared_ptr<ARDOUR::MidiModel> _model;
	Events                               _events;
	PgmChanges                           _pgm_changes;
	ArdourCanvas::CanvasNote**           _active_notes;
	ArdourCanvas::Group*                 _note_group;
	ARDOUR::MidiModel::DeltaCommand*     _delta_command;

	MouseState _mouse_state;
	int _pressed_button;

	typedef std::set<ArdourCanvas::CanvasNoteEvent*> Selection;
	/// Currently selected CanvasNoteEvents
	Selection _selection;

	/** New notes (created in the current command) which should be selected
	 * when they appear after the command is applied. */
	std::set< boost::shared_ptr<NoteType> > _marked_for_selection;

	std::vector<NoteResizeData *> _resize_data;
};

#endif /* __gtk_ardour_midi_region_view_h__ */
