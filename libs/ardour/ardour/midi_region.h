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

class XMLNode;

namespace ARDOUR {

class Route;
class Playlist;
class Session;
class MidiFilter;
class MidiSource;
class MidiBuffer;

class MidiRegion : public Region
{
  public:
	MidiRegion (MidiSource&, jack_nframes_t start, jack_nframes_t length, bool announce = true);
	MidiRegion (MidiSource&, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	MidiRegion (SourceList &, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	MidiRegion (const MidiRegion&, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	MidiRegion (const MidiRegion&);
	MidiRegion (MidiSource&, const XMLNode&);
	MidiRegion (SourceList &, const XMLNode&);
	~MidiRegion();

	MidiSource& midi_source (uint32_t n=0) const;

	jack_nframes_t read_at (MidiBuffer& out,
			jack_nframes_t position,
			jack_nframes_t cnt, 
			uint32_t       chan_n      = 0,
			jack_nframes_t read_frames = 0,
			jack_nframes_t skip_frames = 0) const;

	jack_nframes_t master_read_at (MidiBuffer& buf,
			jack_nframes_t position,
			jack_nframes_t cnt,
			uint32_t chan_n=0) const;

	XMLNode& state (bool);
	int      set_state (const XMLNode&);

	int separate_by_channel (ARDOUR::Session&, vector<MidiRegion*>&) const;

	UndoAction get_memento() const;

  private:
	friend class Playlist;

  private:
	StateManager::State* state_factory (std::string why) const;
	Change restore_state (StateManager::State&);

	jack_nframes_t _read_at (const SourceList&, MidiBuffer& buf,
		jack_nframes_t position,
		jack_nframes_t cnt, 
		uint32_t chan_n = 0,
		jack_nframes_t read_frames = 0,
		jack_nframes_t skip_frames = 0) const;

	void recompute_at_start ();
	void recompute_at_end ();
};

} /* namespace ARDOUR */


#endif /* __ardour_midi_region_h__ */
