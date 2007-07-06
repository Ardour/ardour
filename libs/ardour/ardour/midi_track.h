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

#include <ardour/track.h>
#include <ardour/midi_ring_buffer.h>

namespace ARDOUR
{

class Session;
class MidiDiskstream;
class MidiPlaylist;
class RouteGroup;

class MidiTrack : public Track
{
public:
	MidiTrack (Session&, string name, Route::Flag f = Route::Flag (0), TrackMode m = Normal);
	MidiTrack (Session&, const XMLNode&);
	~MidiTrack ();
	
	int roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
		nframes_t offset, int declick, bool can_record, bool rec_monitors_input);
	
	int no_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
		nframes_t offset, bool state_changing, bool can_record, bool rec_monitors_input);
	
	int silent_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
		nframes_t offset, bool can_record, bool rec_monitors_input);

	void process_output_buffers (BufferSet& bufs,
				     nframes_t start_frame, nframes_t end_frame,
				     nframes_t nframes, nframes_t offset, bool with_redirects, int declick,
				     bool meter);

	boost::shared_ptr<MidiDiskstream> midi_diskstream() const;

	int use_diskstream (string name);
	int use_diskstream (const PBD::ID& id);

	int set_mode (TrackMode m);

	void set_latency_delay (nframes_t);

	int export_stuff (BufferSet& bufs,
		nframes_t nframes, nframes_t end_frame);

	void freeze (InterThreadInfo&);
	void unfreeze ();

	void bounce (InterThreadInfo&);
	void bounce_range (nframes_t start, nframes_t end, InterThreadInfo&);

	int set_state(const XMLNode& node);

	bool write_immediate_event(size_t size, const Byte* buf);
	
	struct MidiControl : public AutomationControl {
	    MidiControl(boost::shared_ptr<MidiTrack> route, boost::shared_ptr<AutomationList> al)
			: AutomationControl (route->session(), al, al->parameter().to_string())
			, _route (route)
		{}
	 
	    void set_value (float val);
   
		boost::weak_ptr<MidiTrack> _route;
	};

protected:
	XMLNode& state (bool full);
	
	int _set_state (const XMLNode&, bool call_base);

private:

	void write_controller_messages(MidiBuffer& buf,
			nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset);

	int set_diskstream (boost::shared_ptr<MidiDiskstream> ds);
	void set_state_part_two ();
	void set_state_part_three ();

	MidiRingBuffer _immediate_events;
};

} /* namespace ARDOUR*/

#endif /* __ardour_midi_track_h__ */
