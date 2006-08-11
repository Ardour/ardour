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

#ifndef __ardour_midi_playlist_h__
#define __ardour_midi_playlist_h__

#include <vector>
#include <list>

#include <ardour/ardour.h>
#include <ardour/playlist.h>

namespace ARDOUR
{

class Session;
class Region;
class MidiRegion;
class Source;

class MidiPlaylist : public ARDOUR::Playlist
{
private:

	struct State : public ARDOUR::StateManager::State
	{
		RegionList regions;
		std::list<UndoAction> region_states;

		State (std::string why) : ARDOUR::StateManager::State (why)
		{}
		~State ();
	};

public:
	MidiPlaylist (Session&, const XMLNode&, bool hidden = false);
	MidiPlaylist (Session&, string name, bool hidden = false);
	MidiPlaylist (const MidiPlaylist&, string name, bool hidden = false);
	MidiPlaylist (const MidiPlaylist&, jack_nframes_t start, jack_nframes_t cnt,
	              string name, bool hidden = false);

	jack_nframes_t read (RawMidi *dst, RawMidi *mixdown,
	                     jack_nframes_t start, jack_nframes_t cnt, uint32_t chan_n=0);

	int set_state (const XMLNode&);
	UndoAction get_memento() const;

	template<class T>
	void apply_to_history (T& obj, void (T::*method)(const ARDOUR::StateManager::StateMap&, state_id_t))
	{
		RegionLock rlock (this);
		(obj.*method) (states, _current_state_id);
	}

	bool destroy_region (Region*);

	void get_equivalent_regions (const MidiRegion&, std::vector<MidiRegion*>&);
	void get_region_list_equivalent_regions (const MidiRegion&, std::vector<MidiRegion*>&);

	void drop_all_states ();

protected:

	/* state management */

	StateManager::State* state_factory (std::string) const;
	Change restore_state (StateManager::State&);
	void send_state_change (Change);

	/* playlist "callbacks" */
	void flush_notifications ();

	void finalize_split_region (Region *orig, Region *left, Region *right);

	void refresh_dependents (Region& region);
	void check_dependents (Region& region, bool norefresh);
	void remove_dependents (Region& region);

protected:
	~MidiPlaylist (); /* public should use unref() */

private:
	XMLNode& state (bool full_state);
	void dump () const;

	bool region_changed (Change, Region*);
};

} /* namespace ARDOUR */

#endif	/* __ardour_midi_playlist_h__ */


