/*
 * Copyright (C) 2011-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_midi_playlist_source_h__
#define __ardour_midi_playlist_source_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/ardour.h"
#include "ardour/midi_source.h"
#include "ardour/playlist_source.h"

namespace ARDOUR {

class MidiPlaylist;

class LIBARDOUR_API MidiPlaylistSource : public MidiSource, public PlaylistSource {
public:
	virtual ~MidiPlaylistSource ();

	bool empty() const;
	samplecnt_t length (samplepos_t) const;

	samplecnt_t read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const;
	samplecnt_t write_unlocked (Sample *src, samplecnt_t cnt);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	void append_event_beats(const Glib::Threads::Mutex::Lock& lock, const Evoral::Event<Temporal::Beats>& ev);
	void append_event_samples(const Glib::Threads::Mutex::Lock& lock, const Evoral::Event<samplepos_t>& ev, samplepos_t source_start);
	void load_model(const Glib::Threads::Mutex::Lock& lock, bool force_reload=false);
	void destroy_model(const Glib::Threads::Mutex::Lock& lock);

protected:
	friend class SourceFactory;

	MidiPlaylistSource (Session&, const PBD::ID& orig, const std::string& name, boost::shared_ptr<MidiPlaylist>, uint32_t chn,
	                    sampleoffset_t begin, samplecnt_t len, Source::Flag flags);
	MidiPlaylistSource (Session&, const XMLNode&);


	void flush_midi(const Lock& lock);

	samplecnt_t read_unlocked (const Lock&                    lock,
	                          Evoral::EventSink<samplepos_t>& dst,
	                          samplepos_t                     position,
	                          samplepos_t                     start,
	                          samplecnt_t                     cnt,
	                          Evoral::Range<samplepos_t>*     loop_range,
	                          MidiStateTracker*              tracker,
	                          MidiChannelFilter*             filter) const;

	samplecnt_t write_unlocked (const Lock&                 lock,
	                           MidiRingBuffer<samplepos_t>& dst,
	                           samplepos_t                  position,
	                           samplecnt_t                  cnt);

private:
	int set_state (const XMLNode&, int version, bool with_descendants);
	samplecnt_t _length;
};

} /* namespace */

#endif /* __ardour_midi_playlist_source_h__ */
