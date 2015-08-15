/*
    Copyright (C) 2000-2006 Paul Davis

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

    $Id: midiregion.h 733 2006-08-01 17:19:38Z drobilla $
*/

#ifndef __ardour_midi_region_h__
#define __ardour_midi_region_h__

#include <vector>

#include "evoral/Beats.hpp"
#include "evoral/Range.hpp"

#include "pbd/string_convert.h"

#include "ardour/ardour.h"
#include "ardour/midi_cursor.h"
#include "ardour/region.h"

class XMLNode;

namespace ARDOUR {
	namespace Properties {
		LIBARDOUR_API extern PBD::PropertyDescriptor<double> start_beats;
		LIBARDOUR_API extern PBD::PropertyDescriptor<double> length_beats;
	}
}

namespace Evoral {
template<typename Time> class EventSink;
}

namespace ARDOUR {

class MidiChannelFilter;
class MidiFilter;
class MidiModel;
class MidiSource;
class MidiStateTracker;
class Playlist;
class Route;
class Session;

template<typename T> class MidiRingBuffer;

class LIBARDOUR_API MidiRegion : public Region
{
  public:
	static void make_property_quarks ();

	~MidiRegion();

	bool do_export (std::string path) const;

	boost::shared_ptr<MidiRegion> clone (std::string path = std::string()) const;
	boost::shared_ptr<MidiRegion> clone (boost::shared_ptr<MidiSource>) const;

	boost::shared_ptr<MidiSource> midi_source (uint32_t n=0) const;

	/* Stub Readable interface */
	virtual framecnt_t read (Sample*, framepos_t /*pos*/, framecnt_t /*cnt*/, int /*channel*/) const { return 0; }
	virtual framecnt_t readable_length() const { return length(); }

	framecnt_t read_at (Evoral::EventSink<framepos_t>& dst,
	                    framepos_t position,
	                    framecnt_t dur,
	                    Evoral::Range<framepos_t>* loop_range,
	                    MidiCursor& cursor,
	                    uint32_t  chan_n = 0,
	                    NoteMode  mode = Sustained,
	                    MidiStateTracker* tracker = 0,
	                    MidiChannelFilter* filter = 0) const;

	framecnt_t master_read_at (MidiRingBuffer<framepos_t>& dst,
	                           framepos_t position,
	                           framecnt_t dur,
	                           Evoral::Range<framepos_t>* loop_range,
	                           MidiCursor& cursor,
	                           uint32_t  chan_n = 0,
	                           NoteMode  mode = Sustained) const;

	XMLNode& state ();
	int      set_state (const XMLNode&, int version);

	int separate_by_channel (ARDOUR::Session&, std::vector< boost::shared_ptr<Region> >&) const;

	/* automation */

	boost::shared_ptr<Evoral::Control> control(const Evoral::Parameter& id, bool create=false);

	virtual boost::shared_ptr<const Evoral::Control> control(const Evoral::Parameter& id) const;

	/* export */

	boost::shared_ptr<MidiModel> model();
	boost::shared_ptr<const MidiModel> model() const;

	void fix_negative_start ();
	double start_beats () const {return _start_beats; }
	double length_beats () const {return _length_beats; }

	void clobber_sources (boost::shared_ptr<MidiSource> source);

  protected:

	virtual bool can_trim_start_before_source_start () const {
		return true;
	}

  private:
	friend class RegionFactory;
	PBD::Property<double> _start_beats;
	PBD::Property<double> _length_beats;

	MidiRegion (const SourceList&);
	MidiRegion (boost::shared_ptr<const MidiRegion>);
	MidiRegion (boost::shared_ptr<const MidiRegion>, ARDOUR::MusicFrame offset);

	framecnt_t _read_at (const SourceList&, Evoral::EventSink<framepos_t>& dst,
	                     framepos_t position,
	                     framecnt_t dur,
	                     Evoral::Range<framepos_t>* loop_range,
	                     MidiCursor& cursor,
	                     uint32_t chan_n = 0,
	                     NoteMode mode = Sustained,
	                     MidiStateTracker* tracker = 0,
	                     MidiChannelFilter* filter = 0) const;

	void register_properties ();
	void post_set (const PBD::PropertyChange&);

	void recompute_at_start ();
	void recompute_at_end ();

	void set_position_internal (framepos_t pos, bool allow_bbt_recompute, const int32_t sub_num);
	void set_position_music_internal (double qn);
	void set_length_internal (framecnt_t len, const int32_t sub_num);
	void set_start_internal (framecnt_t, const int32_t sub_num);
	void trim_to_internal (framepos_t position, framecnt_t length, const int32_t sub_num);
	void update_length_beats (const int32_t sub_num);

	void model_changed ();
	void model_shifted (double qn_distance);
	void model_automation_state_changed (Evoral::Parameter const &);

	void set_start_beats_from_start_frames ();
	void update_after_tempo_map_change (bool send_change = true);

	std::set<Evoral::Parameter> _filtered_parameters; ///< parameters that we ask our source not to return when reading
	PBD::ScopedConnection _model_connection;
	PBD::ScopedConnection _model_shift_connection;
	PBD::ScopedConnection _source_connection;
	PBD::ScopedConnection _model_contents_connection;
	bool _ignore_shift;
};

} /* namespace ARDOUR */


#endif /* __ardour_midi_region_h__ */
