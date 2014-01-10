/*
    Copyright (C) 2006 Paul Davis
    Author: David Robillard

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

#include "pbd/ffs.h"

#include "ardour/track.h"
#include "ardour/midi_ring_buffer.h"

namespace ARDOUR
{

class InterThreadInfo;
class MidiDiskstream;
class MidiPlaylist;
class RouteGroup;
class SMFSource;
class Session;

class LIBARDOUR_API MidiTrack : public Track
{
public:
	MidiTrack (Session&, string name, Route::Flag f = Route::Flag (0), TrackMode m = Normal);
	~MidiTrack ();

	int init ();

	int roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler);

	void realtime_handle_transport_stopped ();
	void realtime_locate ();

	boost::shared_ptr<Diskstream> create_diskstream ();
	void set_diskstream (boost::shared_ptr<Diskstream>);
	void set_record_enabled (bool yn, void *src);

	DataType data_type () const {
		return DataType::MIDI;
	}

	void freeze_me (InterThreadInfo&);
	void unfreeze ();

	bool bounceable (boost::shared_ptr<Processor>, bool) const { return false; }
	boost::shared_ptr<Region> bounce (InterThreadInfo&);
	boost::shared_ptr<Region> bounce_range (framepos_t                   start,
	                                        framepos_t                   end,
	                                        InterThreadInfo&             iti,
	                                        boost::shared_ptr<Processor> endpoint,
	                                        bool                         include_endpoint);

	int export_stuff (BufferSet&                   bufs,
	                  framepos_t                   start_frame,
	                  framecnt_t                   end_frame,
	                  boost::shared_ptr<Processor> endpoint,
	                  bool                         include_endpoint,
	                  bool                         for_export);

	int set_state (const XMLNode&, int version);

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

	std::string describe_parameter (Evoral::Parameter param);

	bool step_editing() const { return _step_editing; }
	void set_step_editing (bool yn);
	MidiRingBuffer<framepos_t>& step_edit_ring_buffer() { return _step_edit_ring_buffer; }

	PBD::Signal1<void,bool> StepEditStatusChange;

	boost::shared_ptr<SMFSource> write_source (uint32_t n = 0);

	/** Channel filtering mode.
	 * @param mask If mode is FilterChannels, each bit represents a midi channel:
	 *     bit 0 = channel 0, bit 1 = channel 1 etc. the read and write methods will only
	 *     process events whose channel bit is 1.
	 *     If mode is ForceChannel, mask is simply a channel number which all events will
	 *     be forced to while reading.
	 */
        void set_capture_channel_mode (ChannelMode mode, uint16_t mask);
        void set_playback_channel_mode (ChannelMode mode, uint16_t mask);
        void set_playback_channel_mask (uint16_t mask);
        void set_capture_channel_mask (uint16_t mask);

	ChannelMode get_playback_channel_mode() const {
		return static_cast<ChannelMode>((g_atomic_int_get(&_playback_channel_mask) & 0xffff0000) >> 16);
	}
        uint16_t get_playback_channel_mask() const {
		return g_atomic_int_get(&_playback_channel_mask) & 0x0000ffff;
	}
	ChannelMode get_capture_channel_mode() const {
		return static_cast<ChannelMode>((g_atomic_int_get(&_capture_channel_mask) & 0xffff0000) >> 16);
	}
        uint16_t get_capture_channel_mask() const {
		return g_atomic_int_get(&_capture_channel_mask) & 0x0000ffff;
	}

	boost::shared_ptr<MidiPlaylist> midi_playlist ();

        PBD::Signal0<void> PlaybackChannelMaskChanged;
        PBD::Signal0<void> PlaybackChannelModeChanged;
        PBD::Signal0<void> CaptureChannelMaskChanged;
        PBD::Signal0<void> CaptureChannelModeChanged;
    
	PBD::Signal1<void, boost::weak_ptr<MidiSource> > DataRecorded;
	boost::shared_ptr<MidiBuffer> get_gui_feed_buffer () const;

	void set_monitoring (MonitorChoice);
	MonitorState monitoring_state () const;

	void set_input_active (bool);
	bool input_active () const;
	PBD::Signal0<void> InputActiveChanged;

protected:
	XMLNode& state (bool full);

	void act_on_mute ();

private:
	MidiRingBuffer<framepos_t> _immediate_events;
	MidiRingBuffer<framepos_t> _step_edit_ring_buffer;
	NoteMode                   _note_mode;
	bool                       _step_editing;
	bool                       _input_active;
        uint32_t                   _playback_channel_mask; // 16 bits mode, 16 bits mask
        uint32_t                   _capture_channel_mask; // 16 bits mode, 16 bits mask

	virtual boost::shared_ptr<Diskstream> diskstream_factory (XMLNode const &);
	
	boost::shared_ptr<MidiDiskstream> midi_diskstream () const;

	void write_out_of_band_data (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, framecnt_t nframes);

	void set_state_part_two ();
	void set_state_part_three ();


	int no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, bool state_changing);
	void push_midi_input_to_step_edit_ringbuffer (framecnt_t nframes);

	void diskstream_data_recorded (boost::weak_ptr<MidiSource>);
	PBD::ScopedConnection _diskstream_data_recorded_connection;

	void track_input_active (IOChange, void*);
	void map_input_active (bool);

        void filter_channels (BufferSet& bufs, ChannelMode mode, uint32_t mask); 

/* if mode is ForceChannel, force mask to the lowest set channel or 1 if no
 * channels are set.
 */
#define force_mask(mode,mask) (((mode) == ForceChannel) ? (((mask) ? (1<<(PBD::ffs((mask))-1)) : 1)) : mask)

	void _set_playback_channel_mode(ChannelMode mode, uint16_t mask) {
		mask = force_mask (mode, mask);
		g_atomic_int_set(&_playback_channel_mask, (uint32_t(mode) << 16) | uint32_t(mask));
	}
        void _set_playback_channel_mask (uint16_t mask) {
		mask = force_mask (get_playback_channel_mode(), mask);
		g_atomic_int_set(&_playback_channel_mask, (uint32_t(get_playback_channel_mode()) << 16) | uint32_t(mask));
	}
	void _set_capture_channel_mode(ChannelMode mode, uint16_t mask) {
		mask = force_mask (mode, mask);
		g_atomic_int_set(&_capture_channel_mask, (uint32_t(mode) << 16) | uint32_t(mask));
	}
        void _set_capture_channel_mask (uint16_t mask) {
		mask = force_mask (get_capture_channel_mode(), mask);
		g_atomic_int_set(&_capture_channel_mask, (uint32_t(get_capture_channel_mode()) << 16) | uint32_t(mask));
	}

#undef force_mask
};

} /* namespace ARDOUR*/

#endif /* __ardour_midi_track_h__ */
