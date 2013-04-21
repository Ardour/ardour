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
#include "ardour/operation.h"
#include "ardour/freezable.h"

namespace ARDOUR {

class Session;
class AudioDiskstream;
class AudioPlaylist;
class RouteGroup;
class AudioFileSource;

 class AudioTrack : public Track, public Freezable
{
  public:
	AudioTrack (Session&, 
		    std::string name, 
		    Route::Flag f = Route::Flag (0), 
		    TrackMode m = Normal);
	~AudioTrack ();

	int set_mode (TrackMode m);
	bool can_use_mode (TrackMode m, bool& bounce_required);

	int roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
	          int declick, bool& need_butler);

	boost::shared_ptr<Diskstream> create_diskstream ();
	void set_diskstream (boost::shared_ptr<Diskstream>);

	DataType data_type () const {
		return DataType::AUDIO;
	}

	//Operation
	void apply(Operation *op);
	void disapply(Operation *op);

	//Recording
	void set_record_enabled (bool yn, void *src);
	void prep_record_enabled (bool yn, void *src);

	//FreezeState
	FreezeState freeze_state() const;
	void freeze_me (InterThreadInfo&);
	void unfreeze ();

	bool bounceable (boost::shared_ptr<Processor>, bool include_endpoint) const;
	boost::shared_ptr<Region> bounce (InterThreadInfo&);
	boost::shared_ptr<Region> bounce_range (framepos_t start, framepos_t end, InterThreadInfo&, 
						boost::shared_ptr<Processor> endpoint, bool include_endpoint);
	int export_stuff (BufferSet& bufs, framepos_t start_frame, framecnt_t nframes,
			  boost::shared_ptr<Processor> endpoint, bool include_endpoint, bool for_export);

	int set_state (const XMLNode&, int version);

	boost::shared_ptr<AudioFileSource> write_source (uint32_t n = 0);

  protected:
	boost::shared_ptr<AudioDiskstream> audio_diskstream () const;
	XMLNode& state (bool full);
	FreezeRecord          _freeze_record;

  private:

	boost::shared_ptr<Diskstream> diskstream_factory (XMLNode const &);
	
	int  deprecated_use_diskstream_connections ();
	void set_state_part_two ();
	void set_state_part_three ();
};

} // namespace ARDOUR

#endif /* __ardour_audio_track_h__ */
