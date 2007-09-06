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

#include <pbd/fastlog.h>
#include <pbd/undo.h>

#include <ardour/ardour.h>
#include <ardour/region.h>
#include <ardour/gain.h>
#include <ardour/logcurve.h>
#include <ardour/export.h>
#include <ardour/midi_source.h>

class XMLNode;

namespace ARDOUR {

class Route;
class Playlist;
class Session;
class MidiFilter;
class MidiSource;
class MidiRingBuffer;

class MidiRegion : public Region
{
  public:
	~MidiRegion();

	boost::shared_ptr<MidiSource> midi_source (uint32_t n=0) const;

	nframes_t read_at (MidiRingBuffer& dst,
			   nframes_t position,
			   nframes_t dur, 
			   uint32_t  chan_n = 0,
			   NoteMode  mode = Sustained) const;

	nframes_t master_read_at (MidiRingBuffer& dst,
			nframes_t position,
			nframes_t dur,
			uint32_t  chan_n = 0,
			NoteMode  mode = Sustained) const;

	XMLNode& state (bool);
	int      set_state (const XMLNode&);

	int separate_by_channel (ARDOUR::Session&, vector<MidiRegion*>&) const;

	UndoAction get_memento() const;

	// Act as a proxy for MidiModel automation stuff (for CC)
	// Yep, this is pretty ugly...
	Controls&       controls()       { return midi_source()->model()->controls(); }
	const Controls& controls() const { return midi_source()->model()->controls(); }
	
	boost::shared_ptr<AutomationControl> control(Parameter id, bool create_if_missing=false)
			{ return midi_source()->model()->control(id, create_if_missing); }

	boost::shared_ptr<const AutomationControl> control(Parameter id) const
			{ return midi_source()->model()->control(id); }

  private:
	friend class RegionFactory;

	MidiRegion (boost::shared_ptr<MidiSource>, nframes_t start, nframes_t length);
	MidiRegion (boost::shared_ptr<MidiSource>, nframes_t start, nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags);
	MidiRegion (SourceList &, nframes_t start, nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags);
	MidiRegion (boost::shared_ptr<const MidiRegion>, nframes_t start, nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags);
	MidiRegion (boost::shared_ptr<const MidiRegion>);
	MidiRegion (boost::shared_ptr<MidiSource>, const XMLNode&);
	MidiRegion (SourceList &, const XMLNode&);

  private:
	nframes_t _read_at (const SourceList&, MidiRingBuffer& dst,
			    nframes_t position,
			    nframes_t dur, 
			    uint32_t chan_n = 0,
				NoteMode mode = Sustained) const;

	void recompute_at_start ();
	void recompute_at_end ();

	void switch_source(boost::shared_ptr<Source> source);

  protected:

	int set_live_state (const XMLNode&, Change&, bool send);
};

} /* namespace ARDOUR */


#endif /* __ardour_midi_region_h__ */
