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
	
	int set_name (string str, void *src);

	int roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, 
		jack_nframes_t offset, int declick, bool can_record, bool rec_monitors_input);
	
	int no_roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, 
		jack_nframes_t offset, bool state_changing, bool can_record, bool rec_monitors_input);
	
	int silent_roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, 
		jack_nframes_t offset, bool can_record, bool rec_monitors_input);

	void set_record_enable (bool yn, void *src);

	MidiDiskstream& midi_diskstream() const;

	int use_diskstream (string name);
	int use_diskstream (const PBD::ID& id);

	void set_mode (TrackMode m);

	void set_latency_delay (jack_nframes_t);

	int export_stuff (vector<unsigned char*>& buffers, char * workbuf, uint32_t nbufs,
		jack_nframes_t nframes, jack_nframes_t end_frame);

	void freeze (InterThreadInfo&);
	void unfreeze ();

	void bounce (InterThreadInfo&);
	void bounce_range (jack_nframes_t start, jack_nframes_t end, InterThreadInfo&);

	int set_state(const XMLNode& node);

	bool record_enabled() const;

protected:
	XMLNode& state (bool full);

	void passthru_silence (jack_nframes_t start_frame, jack_nframes_t end_frame,
	                       jack_nframes_t nframes, jack_nframes_t offset, int declick,
	                       bool meter);

	uint32_t n_process_buffers ();

private:
	int set_diskstream (MidiDiskstream&, void *);

	void set_state_part_two ();
	void set_state_part_three ();
};

} /* namespace ARDOUR*/

#endif /* __ardour_midi_track_h__ */
