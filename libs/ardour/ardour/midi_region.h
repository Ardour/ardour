/*
    Copyright (C) 2006 Paul Davis 
	Written by Dave Robillard, 2006

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

#ifndef __ardour_midi_region_h__
#define __ardour_midi_region_h__

#include <vector>

#include <pbd/fastlog.h>
#include <pbd/undo.h>

#include <ardour/ardour.h>
#include <ardour/region.h>
#include <ardour/export.h>

class XMLNode;

namespace ARDOUR {

class Route;
class Playlist;
class Session;
class MidiFilter;
class MidiSource;

struct MidiRegionState : public RegionState 
{
    MidiRegionState (std::string why);

};

class MidiRegion : public Region
{
  public:
	typedef vector<MidiSource *> SourceList;

	MidiRegion (MidiSource&, jack_nframes_t start, jack_nframes_t length, bool announce = true);
	MidiRegion (MidiSource&, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	MidiRegion (SourceList &, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	MidiRegion (const MidiRegion&, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	MidiRegion (const MidiRegion&);
	MidiRegion (MidiSource&, const XMLNode&);
	MidiRegion (SourceList &, const XMLNode&);
	~MidiRegion();

	bool region_list_equivalent (const MidiRegion&) const ;
	bool source_equivalent (const MidiRegion&) const;
	bool equivalent (const MidiRegion&) const;
	bool size_equivalent (const MidiRegion&) const;
	bool overlap_equivalent (const MidiRegion&) const;

	bool speed_mismatch (float) const;

	void lock_sources ();
	void unlock_sources ();
	MidiSource& source (uint32_t n=0) const { if (n < sources.size()) return *sources[n]; else return *sources[0]; } 

	uint32_t n_channels() { return sources.size(); }
	vector<string> master_source_names();
	
	bool captured() const { return !(_flags & (Region::Flag (Region::Import|Region::External))); }

	virtual jack_nframes_t read_at (unsigned char *buf, unsigned char *mixdown_buffer, 
					char * workbuf, jack_nframes_t position, jack_nframes_t cnt, 
					uint32_t chan_n = 0,
					jack_nframes_t read_frames = 0,
					jack_nframes_t skip_frames = 0) const;

	jack_nframes_t master_read_at (unsigned char *buf, unsigned char *mixdown_buffer, 
				       char * workbuf, jack_nframes_t position, jack_nframes_t cnt, uint32_t chan_n=0) const;


	XMLNode& state (bool);
	XMLNode& get_state ();
	int      set_state (const XMLNode&);

	enum FadeShape {
		Linear,
		Fast,
		Slow,
		LogA,
		LogB,

	};

	int separate_by_channel (ARDOUR::Session&, vector<MidiRegion*>&) const;

	uint32_t read_data_count() const { return _read_data_count; }

	ARDOUR::Playlist* playlist() const { return _playlist; }

	UndoAction get_memento() const;

	/* export */

	//int exportme (ARDOUR::Session&, ARDOUR::AudioExportSpecification&);

	Region* get_parent();

  private:
	friend class Playlist;

  private:
	SourceList        sources;
	SourceList        master_sources; /* used when timefx are applied, so 
					     we can always use the original
					     source.
					  */
	StateManager::State* state_factory (std::string why) const;
	Change restore_state (StateManager::State&);

	bool copied() const { return _flags & Copied; }
	void maybe_uncopy ();
	void rename_after_first_edit ();

	jack_nframes_t _read_at (const SourceList&, unsigned char *buf, unsigned char *mixdown_buffer, 
				 char * workbuf, jack_nframes_t position, jack_nframes_t cnt, 
				 uint32_t chan_n = 0,
				 jack_nframes_t read_frames = 0,
				 jack_nframes_t skip_frames = 0) const;

	bool verify_start (jack_nframes_t position);
	bool verify_length (jack_nframes_t position);
	bool verify_start_mutable (jack_nframes_t& start);
	bool verify_start_and_length (jack_nframes_t start, jack_nframes_t length);

	void recompute_at_start() {}
	void recompute_at_end() {}

	void source_deleted (Source*);
};

} /* namespace ARDOUR */


#endif /* __ardour_midi_region_h__ */
