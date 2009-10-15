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

*/

#ifndef __ardour_audio_track_h__
#define __ardour_audio_track_h__

#include "ardour/track.h"

namespace ARDOUR {

class Session;
class AudioDiskstream;
class AudioPlaylist;
class RouteGroup;

class AudioTrack : public Track
{
  public:
	AudioTrack (Session&, std::string name, Route::Flag f = Route::Flag (0), TrackMode m = Normal);
	AudioTrack (Session&, const XMLNode&, int);
	~AudioTrack ();

	int set_mode (TrackMode m);
	bool can_use_mode (TrackMode m, bool& bounce_required);

	int roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame,
			int declick, bool can_record, bool rec_monitors_input);

	boost::shared_ptr<AudioDiskstream> audio_diskstream() const;

	int use_diskstream (std::string name);
	int use_diskstream (const PBD::ID& id);

	int export_stuff (BufferSet& bufs, sframes_t start_frame, nframes_t nframes, bool enable_processing = true);

	void freeze (InterThreadInfo&);
	void unfreeze ();

	boost::shared_ptr<Region> bounce (InterThreadInfo&);
	boost::shared_ptr<Region> bounce_range (nframes_t start, nframes_t end, InterThreadInfo&, bool enable_processing);

	int set_state(const XMLNode&, int version);

  protected:
	XMLNode& state (bool full);
	
	int _set_state (const XMLNode&, int, bool call_base);

  private:
	int  set_diskstream (boost::shared_ptr<AudioDiskstream>, void *);
	int  deprecated_use_diskstream_connections ();
	void use_new_diskstream ();
	void set_state_part_two ();
	void set_state_part_three ();
};

} // namespace ARDOUR

#endif /* __ardour_audio_track_h__ */
