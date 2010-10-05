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

#include "pbd/fastlog.h"
#include "pbd/undo.h"

#include "ardour/ardour.h"
#include "ardour/gain.h"
#include "ardour/logcurve.h"
#include "ardour/midi_model.h"
#include "ardour/midi_source.h"
#include "ardour/region.h"

class XMLNode;

namespace ARDOUR {
        namespace Properties {
                /* this is pseudo-property: nothing has this as an actual
                   property, but it allows us to signal changes to the
                   MidiModel used by the MidiRegion 
                */
                extern PBD::PropertyDescriptor<void*> midi_data; 
        }
}

namespace ARDOUR {

class Route;
class Playlist;
class Session;
class MidiFilter;
class MidiSource;
class MidiStateTracker;
template<typename T> class MidiRingBuffer;

class MidiRegion : public Region
{
  public:
	static void make_property_quarks ();

	~MidiRegion();

        boost::shared_ptr<MidiRegion> clone ();
        
	boost::shared_ptr<MidiSource> midi_source (uint32_t n=0) const;

	/* Stub Readable interface */
	virtual framecnt_t read (Sample*, framepos_t /*pos*/, framecnt_t /*cnt*/, int /*channel*/) const { return 0; }
	virtual framecnt_t readable_length() const { return length(); }

	framecnt_t read_at (Evoral::EventSink<nframes_t>& dst,
			    framepos_t position,
			    framecnt_t dur,
			    uint32_t  chan_n = 0,
			    NoteMode  mode = Sustained,
			    MidiStateTracker* tracker = 0) const;
	
	framepos_t master_read_at (MidiRingBuffer<nframes_t>& dst,
				   framepos_t position,
				   framecnt_t dur,
				   uint32_t  chan_n = 0,
				   NoteMode  mode = Sustained) const;

	XMLNode& state ();
	int      set_state (const XMLNode&, int version);
	
	int separate_by_channel (ARDOUR::Session&, std::vector< boost::shared_ptr<Region> >&) const;

	/* automation */
	
	boost::shared_ptr<Evoral::Control>
	control(const Evoral::Parameter& id, bool create=false) {
		return model()->control(id, create);
	}

	virtual boost::shared_ptr<const Evoral::Control>
	control(const Evoral::Parameter& id) const {
		return model()->control(id);
	}

	/* export */

	int exportme (ARDOUR::Session&, ARDOUR::ExportSpecification&);

	boost::shared_ptr<MidiModel> model()             { return midi_source()->model(); }
	boost::shared_ptr<const MidiModel> model() const { return midi_source()->model(); }

  private:
	friend class RegionFactory;

	MidiRegion (const SourceList&);
	MidiRegion (boost::shared_ptr<const MidiRegion>, frameoffset_t offset = 0, bool offset_relative = true);

  private:
	framecnt_t _read_at (const SourceList&, Evoral::EventSink<nframes_t>& dst,
			     framepos_t position,
			     framecnt_t dur,
			     uint32_t chan_n = 0,
			     NoteMode mode = Sustained, 
			     MidiStateTracker* tracker = 0) const;

	void register_properties ();

	void recompute_at_start ();
	void recompute_at_end ();

	void set_position_internal (framepos_t pos, bool allow_bbt_recompute);

	void model_changed ();
	void model_automation_state_changed (Evoral::Parameter const &);
        void model_contents_changed ();

	std::set<Evoral::Parameter> _filtered_parameters; ///< parameters that we ask our source not to return when reading
	PBD::ScopedConnection _model_connection;
	PBD::ScopedConnection _source_connection;
        PBD::ScopedConnection _model_contents_connection;
};

} /* namespace ARDOUR */


#endif /* __ardour_midi_region_h__ */
