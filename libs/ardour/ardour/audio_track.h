/*
    Copyright (C) 2002-2006 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_audio_track_h__
#define __ardour_audio_track_h__

#include <ardour/track.h>

namespace ARDOUR {

class Session;
class AudioDiskstream;
class AudioPlaylist;
class RouteGroup;

class AudioTrack : public Track
{
  public:
	AudioTrack (Session&, string name, Route::Flag f = Route::Flag (0), TrackMode m = Normal);
	AudioTrack (Session&, const XMLNode&);
	~AudioTrack ();

	int set_mode (TrackMode m); 

	int roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
		nframes_t offset, int declick, bool can_record, bool rec_monitors_input);
	
	int no_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
		nframes_t offset, bool state_changing, bool can_record, bool rec_monitors_input);
	
	int silent_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
		nframes_t offset, bool can_record, bool rec_monitors_input);

	boost::shared_ptr<AudioDiskstream> audio_diskstream() const;

	int use_diskstream (string name);
	int use_diskstream (const PBD::ID& id);
	
	int export_stuff (vector<Sample*>& buffers, uint32_t nbufs, nframes_t nframes, nframes_t end_frame);

	void freeze (InterThreadInfo&);
	void unfreeze ();

	void bounce (InterThreadInfo&);
	void bounce_range (nframes_t start, nframes_t end, InterThreadInfo&);

	int set_state(const XMLNode& node);

  protected:
	XMLNode& state (bool full);

	void passthru_silence (nframes_t start_frame, nframes_t end_frame, 
			       nframes_t nframes, nframes_t offset, int declick,
			       bool meter);

	uint32_t n_process_buffers ();

	int _set_state (const XMLNode&, bool call_base);
	
  private:
	int  set_diskstream (boost::shared_ptr<AudioDiskstream>, void *);
	int  deprecated_use_diskstream_connections ();
	void set_state_part_two ();
	void set_state_part_three ();
};

} // namespace ARDOUR

#endif /* __ardour_audio_track_h__ */
