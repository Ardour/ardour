/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_midi_region_h__
#define __ardour_midi_region_h__

#include <vector>

#include "temporal/beats.h"
#include "temporal/range.h"

#include "pbd/string_convert.h"

#include "ardour/ardour.h"
#include "ardour/midi_cursor.h"
#include "ardour/region.h"

class XMLNode;

namespace Evoral {
template<typename Time> class EventSink;
}

namespace ARDOUR {

class MidiChannelFilter;
class MidiFilter;
class MidiModel;
class MidiSource;
class MidiNoteTracker;
class Playlist;
class Route;
class Session;
class ThawList;

template<typename T> class MidiRingBuffer;

class LIBARDOUR_API MidiRegion : public Region
{
  public:
	~MidiRegion();

	std::shared_ptr<MidiRegion> clone (std::string path = std::string()) const;
	std::shared_ptr<MidiRegion> clone (std::shared_ptr<MidiSource>, ThawList* tl = 0) const;

	std::shared_ptr<MidiSource> midi_source (uint32_t n=0) const;

	timecnt_t read_at (Evoral::EventSink<samplepos_t>& dst,
	                   timepos_t const & position,
	                   timecnt_t const & dur,
	                   Temporal::Range* loop_range,
	                   MidiCursor& cursor,
	                   uint32_t  chan_n = 0,
	                   NoteMode  mode = Sustained,
	                   MidiNoteTracker* tracker = 0,
	                   MidiChannelFilter* filter = 0) const;

	timecnt_t master_read_at (MidiRingBuffer<samplepos_t>& dst,
	                          timepos_t const & position,
	                          timecnt_t const & dur,
	                          Temporal::Range* loop_range,
	                          MidiCursor& cursor,
	                          uint32_t  chan_n = 0,
	                          NoteMode  mode = Sustained) const;

	void merge (std::shared_ptr<MidiRegion const>);

	XMLNode& state () const;
	int      set_state (const XMLNode&, int version);

	int separate_by_channel (std::vector< std::shared_ptr<Region> >&) const;

	/* automation */

	std::shared_ptr<Evoral::Control> control(const Evoral::Parameter& id, bool create=false);

	virtual std::shared_ptr<const Evoral::Control> control(const Evoral::Parameter& id) const;

	/* export */

	bool do_export (std::string const& path) const;

	std::shared_ptr<MidiModel> model();
	std::shared_ptr<const MidiModel> model() const;

	void fix_negative_start ();

	int render (Evoral::EventSink<samplepos_t>& dst,
	            uint32_t                        chan_n,
	            NoteMode                        mode,
	            MidiChannelFilter*              filter) const;

	int render_range (Evoral::EventSink<samplepos_t>& dst,
	                  uint32_t                        chan_n,
	                  NoteMode                        mode,
	                  timepos_t const &               read_start,
	                  timecnt_t const &               read_length,
	                  MidiChannelFilter*              filter) const;

	void start_domain_bounce (Temporal::DomainBounceInfo&);
	void finish_domain_bounce (Temporal::DomainBounceInfo&);

  protected:

	virtual bool can_trim_start_before_source_start () const {
		return true;
	}

  private:
	friend class RegionFactory;

	MidiRegion (const SourceList&);
	MidiRegion (std::shared_ptr<const MidiRegion>);
	MidiRegion (std::shared_ptr<const MidiRegion>, timecnt_t const & offset);

	timecnt_t _read_at (const SourceList&, Evoral::EventSink<samplepos_t>& dst,
	                    timepos_t const & position,
	                    timecnt_t const & dur,
	                    Temporal::Range* loop_range,
	                    MidiCursor& cursor,
	                    uint32_t chan_n = 0,
	                    NoteMode mode = Sustained,
	                    MidiNoteTracker* tracker = 0,
	                    MidiChannelFilter* filter = 0) const;

	void register_properties ();

	void recompute_at_start ();
	void recompute_at_end ();

	bool set_name (const std::string & str);

	void model_changed ();
	void model_contents_changed ();
	void model_shifted (timecnt_t qn_distance);
	void model_automation_state_changed (Evoral::Parameter const &);

	std::set<Evoral::Parameter> _filtered_parameters; ///< parameters that we ask our source not to return when reading
	PBD::ScopedConnection _model_connection;
	PBD::ScopedConnection _model_shift_connection;
	PBD::ScopedConnection _model_changed_connection;
	PBD::ScopedConnection _source_connection;
	PBD::ScopedConnection _model_contents_connection;
	bool _ignore_shift;
};

} /* namespace ARDOUR */


#endif /* __ardour_midi_region_h__ */
