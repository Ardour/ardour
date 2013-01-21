/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __ardour_midi_playlist_source_h__
#define __ardour_midi_playlist_source_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/ardour.h"
#include "ardour/midi_source.h"
#include "ardour/playlist_source.h"

namespace ARDOUR {

class MidiPlaylist;

class MidiPlaylistSource : public MidiSource, public PlaylistSource {
public:
	virtual ~MidiPlaylistSource ();

	bool empty() const;
	framecnt_t length (framepos_t) const;

	framecnt_t read_unlocked (Sample *dst, framepos_t start, framecnt_t cnt) const;
	framecnt_t write_unlocked (Sample *src, framecnt_t cnt);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	void append_event_unlocked_beats(const Evoral::Event<Evoral::MusicalTime>& ev);
	void append_event_unlocked_frames(const Evoral::Event<framepos_t>& ev, framepos_t source_start);
	void load_model(bool lock=true, bool force_reload=false);
	void destroy_model();

protected:
	friend class SourceFactory;

	MidiPlaylistSource (Session&, const PBD::ID& orig, const std::string& name, boost::shared_ptr<MidiPlaylist>, uint32_t chn,
	                    frameoffset_t begin, framecnt_t len, Source::Flag flags);
	MidiPlaylistSource (Session&, const XMLNode&);


	void flush_midi();

	framecnt_t read_unlocked (Evoral::EventSink<framepos_t>& dst,
	                          framepos_t                     position,
	                          framepos_t                     start,
	                          framecnt_t                     cnt,
	                          MidiStateTracker*              tracker) const;

	framecnt_t write_unlocked (MidiRingBuffer<framepos_t>& dst,
	                           framepos_t                  position,
	                           framecnt_t                  cnt);

private:
	int set_state (const XMLNode&, int version, bool with_descendants);
	framecnt_t _length;
};

} /* namespace */

#endif /* __ardour_midi_playlist_source_h__ */
