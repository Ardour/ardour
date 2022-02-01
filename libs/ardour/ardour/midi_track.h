/*
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_midi_track_h__
#define __ardour_midi_track_h__

#include "ardour/midi_channel_filter.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/track.h"

namespace ARDOUR
{

class InterThreadInfo;
class MidiPlaylist;
class RouteGroup;
class SMFSource;
class Session;

class LIBARDOUR_API MidiTrack : public Track
{
public:
	MidiTrack (Session&, std::string name = "", TrackMode m = Normal);
	~MidiTrack ();

	int init ();

	void realtime_locate (bool);
	void non_realtime_locate (samplepos_t);

	bool can_be_record_enabled ();
	bool can_be_record_safe ();

	void freeze_me (InterThreadInfo&);
	void unfreeze ();

	bool bounceable (boost::shared_ptr<Processor>, bool) const { return false; }
	boost::shared_ptr<Region> bounce (InterThreadInfo&, std::string const&);
	boost::shared_ptr<Region> bounce_range (samplepos_t                  start,
	                                        samplepos_t                  end,
	                                        InterThreadInfo&             iti,
	                                        boost::shared_ptr<Processor> endpoint,
	                                        bool                         include_endpoint,
	                                        std::string const&           name);

	int export_stuff (BufferSet&                   bufs,
	                  samplepos_t                  start_sample,
	                  samplecnt_t                  end_sample,
	                  boost::shared_ptr<Processor> endpoint,
	                  bool                         include_endpoint,
	                  bool                         for_export,
	                  bool                         for_freeze,
	                  MidiNoteTracker&            tracker);

	int set_state (const XMLNode&, int version);

	void midi_panic(void);
	bool write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf);

	/** A control that will send "immediate" events to a MIDI track when twiddled */
	struct MidiControl : public AutomationControl {
		MidiControl(MidiTrack* route, const Evoral::Parameter& param,
			    boost::shared_ptr<AutomationList> al = boost::shared_ptr<AutomationList>())
			: AutomationControl (route->session(), param, ParameterDescriptor(param), al)
			, _route (route)
		{}

		bool writable() const { return true; }
		void restore_value ();

		MidiTrack* _route;

	private:
		void actually_set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
	};

	virtual void set_parameter_automation_state (Evoral::Parameter param, AutoState);

	NoteMode note_mode() const { return _note_mode; }
	void set_note_mode (NoteMode m);

	std::string describe_parameter (Evoral::Parameter param);

	bool step_editing() const { return _step_editing; }
	void set_step_editing (bool yn);
	MidiRingBuffer<samplepos_t>& step_edit_ring_buffer() { return _step_edit_ring_buffer; }

	PBD::Signal1<void,bool> StepEditStatusChange;

	boost::shared_ptr<SMFSource> write_source (uint32_t n = 0);

	/* Configure capture/playback channels (see MidiChannelFilter). */
	void set_capture_channel_mode (ChannelMode mode, uint16_t mask);
	void set_playback_channel_mode (ChannelMode mode, uint16_t mask);
	void set_playback_channel_mask (uint16_t mask);
	void set_capture_channel_mask (uint16_t mask);

	ChannelMode get_playback_channel_mode() const { return _playback_filter.get_channel_mode(); }
	ChannelMode get_capture_channel_mode()  const { return _capture_filter.get_channel_mode(); }
	uint16_t    get_playback_channel_mask() const { return _playback_filter.get_channel_mask(); }
	uint16_t    get_capture_channel_mask()  const { return _capture_filter.get_channel_mask(); }

	MidiChannelFilter& playback_filter() { return _playback_filter; }
	MidiChannelFilter& capture_filter()  { return _capture_filter; }

	virtual void filter_input (BufferSet& bufs);

	boost::shared_ptr<MidiPlaylist> midi_playlist ();

	PBD::Signal1<void, boost::weak_ptr<MidiSource> > DataRecorded;
	boost::shared_ptr<MidiBuffer> get_gui_feed_buffer () const;

	MonitorState monitoring_state () const;
	MonitorState get_input_monitoring_state (bool recording, bool talkback) const;

	MidiBuffer const& immediate_event_buffer () const { return _immediate_event_buffer; }
	MidiRingBuffer<samplepos_t>& immediate_events () { return _immediate_events; }

	void set_input_active (bool);
	bool input_active () const;
	PBD::Signal0<void> InputActiveChanged;

	void set_restore_pgm_on_load (bool yn);
	bool restore_pgm_on_load () const;

	void realtime_handle_transport_stopped ();
	void region_edited (boost::shared_ptr<Region>);

protected:

	XMLNode& state (bool save_template);

	void act_on_mute ();
	void monitoring_changed (bool, PBD::Controllable::GroupControlDisposition);

	void snapshot_out_of_band_data (samplecnt_t nframes);
	void write_out_of_band_data (BufferSet& bufs, samplecnt_t /* nframes */) const;


private:
	MidiRingBuffer<samplepos_t> _immediate_events;
	MidiBuffer                  _immediate_event_buffer;
	MidiRingBuffer<samplepos_t> _step_edit_ring_buffer;
	NoteMode                    _note_mode;
	bool                        _step_editing;
	bool                        _input_active;
	bool                        _restore_pgm_on_load;
	MidiChannelFilter           _playback_filter;
	MidiChannelFilter           _capture_filter;

	void set_state_part_two ();
	void set_state_part_three ();

	int no_roll_unlocked (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool state_changing);
	void push_midi_input_to_step_edit_ringbuffer (samplecnt_t nframes);

	void track_input_active (IOChange, void*);
	void map_input_active (bool);

	void data_recorded (boost::weak_ptr<MidiSource> src);

	/** Update automation controls to reflect any changes in buffers. */
	void update_controls (BufferSet const& bufs);
	void restore_controls ();

	void playlist_contents_changed ();
	PBD::ScopedConnection playlist_content_change_connection;
};

} /* namespace ARDOUR*/

#endif /* __ardour_midi_track_h__ */
