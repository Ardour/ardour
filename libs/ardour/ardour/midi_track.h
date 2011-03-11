/*
    Copyright (C) 2006 Paul Davis
    Written by Dave Robillard

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

#ifndef __ardour_midi_track_h__
#define __ardour_midi_track_h__

#include "ardour/track.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_state_tracker.h"

namespace ARDOUR
{

class Session;
class MidiDiskstream;
class MidiPlaylist;
class RouteGroup;
class SMFSource;	

class MidiTrack : public Track
{
public:
	MidiTrack (Session&, string name, Route::Flag f = Route::Flag (0), TrackMode m = Normal);
	~MidiTrack ();

	int roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
                  int declick, bool can_record, bool rec_monitors_input, bool& need_butler);

	void realtime_handle_transport_stopped ();

	void use_new_diskstream ();
        void set_diskstream (boost::shared_ptr<Diskstream>);
	void set_record_enabled (bool yn, void *src);

	DataType data_type () const {
		return DataType::MIDI;
	}

	int export_stuff (BufferSet& bufs, framecnt_t nframes, framepos_t end_frame);

	void freeze_me (InterThreadInfo&);
	void unfreeze ();
        
	boost::shared_ptr<Region> bounce (InterThreadInfo&);
	boost::shared_ptr<Region> bounce_range (
			framepos_t start, framepos_t end, InterThreadInfo&, bool enable_processing
		);

	int set_state(const XMLNode&, int version);

	void midi_panic(void);
	bool write_immediate_event(size_t size, const uint8_t* buf);

	/** A control that will send "immediate" events to a MIDI track when twiddled */
	struct MidiControl : public AutomationControl {
		MidiControl(MidiTrack* route, const Evoral::Parameter& param,
			    boost::shared_ptr<AutomationList> al = boost::shared_ptr<AutomationList>())
			: AutomationControl (route->session(), param, al)
			, _route (route)
		{}

		void set_value (double val);

		MidiTrack* _route;
	};

	NoteMode note_mode() const { return _note_mode; }
	void set_note_mode (NoteMode m);

	bool step_editing() const { return _step_editing; }
	void set_step_editing (bool yn);
	MidiRingBuffer<framepos_t>& step_edit_ring_buffer() { return _step_edit_ring_buffer; }

        PBD::Signal1<void,bool> StepEditStatusChange;

	uint8_t default_channel() const { return _default_channel; }
	void set_default_channel (uint8_t chn);

	bool midi_thru() const { return _midi_thru; }
	void set_midi_thru (bool yn);

	boost::shared_ptr<SMFSource> write_source (uint32_t n = 0);
	void set_channel_mode (ChannelMode, uint16_t);
	ChannelMode get_channel_mode ();
	uint16_t get_channel_mask ();
	boost::shared_ptr<MidiPlaylist> midi_playlist ();

	bool bounceable () const {
		return false;
	}
	
	PBD::Signal2<void, boost::shared_ptr<MidiBuffer>, boost::weak_ptr<MidiSource> > DataRecorded;

protected:
	XMLNode& state (bool full);
	
	int _set_state (const XMLNode&, int, bool call_base);
        bool should_monitor () const;
        bool send_silence () const;

  private:
	boost::shared_ptr<MidiDiskstream> midi_diskstream () const;

	void write_out_of_band_data (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, framecnt_t nframes);

	void set_state_part_two ();
	void set_state_part_three ();

	MidiRingBuffer<framepos_t> _immediate_events;
	MidiRingBuffer<framepos_t> _step_edit_ring_buffer;
	NoteMode                  _note_mode;
	bool                      _step_editing;
	uint8_t                   _default_channel;
	bool                      _midi_thru;

	int no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
			bool state_changing, bool can_record, bool rec_monitors_input);
	void push_midi_input_to_step_edit_ringbuffer (framecnt_t nframes);

	void diskstream_data_recorded (boost::shared_ptr<MidiBuffer>, boost::weak_ptr<MidiSource>);
	PBD::ScopedConnection _diskstream_data_recorded_connection;
};

} /* namespace ARDOUR*/

#endif /* __ardour_midi_track_h__ */
