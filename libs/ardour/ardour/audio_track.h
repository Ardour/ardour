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

#include "ardour/interthread_info.h"
#include "ardour/track.h"

namespace ARDOUR {

class Session;
class AudioPlaylist;
class RouteGroup;
class AudioFileSource;

class LIBARDOUR_API AudioTrack : public Track
{
  public:
	AudioTrack (Session&, std::string name, TrackMode m = Normal);
	~AudioTrack ();

	MonitorState get_auto_monitoring_state () const;

	void freeze_me (InterThreadInfo&);
	void unfreeze ();

	bool bounceable (boost::shared_ptr<Processor>, bool include_endpoint) const;
	boost::shared_ptr<Region> bounce (InterThreadInfo&);
	boost::shared_ptr<Region> bounce_range (samplepos_t start, samplepos_t end, InterThreadInfo&,
						boost::shared_ptr<Processor> endpoint, bool include_endpoint);
	int export_stuff (BufferSet& bufs, samplepos_t start_sample, samplecnt_t nframes,
			  boost::shared_ptr<Processor> endpoint, bool include_endpoint, bool for_export, bool for_freeze);

	int set_state (const XMLNode&, int version);

	boost::shared_ptr<AudioFileSource> write_source (uint32_t n = 0);

  protected:
	XMLNode& state (bool save_template);

  private:
	int  deprecated_use_diskstream_connections ();
	void set_state_part_two ();
	void set_state_part_three ();
};

} // namespace ARDOUR

#endif /* __ardour_audio_track_h__ */
