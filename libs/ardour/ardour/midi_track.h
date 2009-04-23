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
		  int declick, bool can_record, bool rec_monitors_input);
	
	int no_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
		     bool state_changing, bool can_record, bool rec_monitors_input);
	
	int silent_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
			 bool can_record, bool rec_monitors_input);

	void process_output_buffers (BufferSet& bufs,
				     nframes_t start_frame, nframes_t end_frame,
				     nframes_t nframes, bool with_redirects, int declick,
				     bool meter);

	boost::shared_ptr<MidiDiskstream> midi_diskstream() const;

	int use_diskstream (string name);
	int use_diskstream (const PBD::ID& id);

	void set_latency_delay (nframes_t);

	int export_stuff (BufferSet& bufs,
		nframes_t nframes, nframes_t end_frame);

	void freeze (InterThreadInfo&);
	void unfreeze ();

	boost::shared_ptr<Region> bounce (InterThreadInfo&);
	boost::shared_ptr<Region>  bounce_range (nframes_t start, nframes_t end, InterThreadInfo&,
						 bool enable_processing);

	int set_state(const XMLNode& node);

	void midi_panic(void);
	bool write_immediate_event(size_t size, const uint8_t* buf);
	
	/** A control that will send "immediate" events to a MIDI track when twiddled */
	struct MidiControl : public AutomationControl {
	    MidiControl(MidiTrack* route, const Evoral::Parameter& param,
				boost::shared_ptr<AutomationList> al = boost::shared_ptr<AutomationList>())
			: AutomationControl (route->session(), param, al)
			, _route (route)
		{}
	 
	    void set_value (float val);
   
		MidiTrack* _route;
	};
	
	NoteMode note_mode() const { return _note_mode; }
	void set_note_mode (NoteMode m);
	
protected:
	XMLNode& state (bool full);
	
	int _set_state (const XMLNode&, bool call_base);

private:
	void write_controller_messages(MidiBuffer& buf, nframes_t start_frame, nframes_t end_frame, nframes_t nframes);

	int set_diskstream (boost::shared_ptr<MidiDiskstream> ds);
	void use_new_diskstream ();
	void set_state_part_two ();
	void set_state_part_three ();

	MidiRingBuffer<nframes_t> _immediate_events;
	NoteMode                  _note_mode;
};

} /* namespace ARDOUR*/

#endif /* __ardour_midi_track_h__ */
