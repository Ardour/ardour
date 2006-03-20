/*
    Copyright (C) 2003 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_audio_playlist_h__
#define __ardour_audio_playlist_h__

#include <vector>
#include <list>

#include <ardour/ardour.h>
#include <ardour/playlist.h>

namespace ARDOUR  {

class Session;
class Region;
class AudioRegion;
class Source;

class AudioPlaylist : public ARDOUR::Playlist
{
  public:
	typedef std::list<Crossfade*> Crossfades;

  private:
	
	struct State : public ARDOUR::StateManager::State {
	    RegionList regions;
	    std::list<UndoAction> region_states;
	    
	    Crossfades crossfades;
	    std::list<UndoAction> crossfade_states;
	    
	    State (std::string why) : ARDOUR::StateManager::State (why) {}
	    ~State ();
	};
	
   public:
	AudioPlaylist (Session&, const XMLNode&, bool hidden = false);
	AudioPlaylist (Session&, string name, bool hidden = false);
	AudioPlaylist (const AudioPlaylist&, string name, bool hidden = false);
	AudioPlaylist (const AudioPlaylist&, jack_nframes_t start, jack_nframes_t cnt, string name, bool hidden = false);

	void clear (bool with_delete = false, bool with_save = true);

        jack_nframes_t read (Sample *dst, Sample *mixdown, float *gain_buffer, char * workbuf, jack_nframes_t start, jack_nframes_t cnt, uint32_t chan_n=0);

	int set_state (const XMLNode&);
	UndoAction get_memento() const;

	sigc::signal<void,Crossfade *> NewCrossfade; 

	template<class T> void foreach_crossfade (T *t, void (T::*func)(Crossfade *));
	void crossfades_at (jack_nframes_t frame, Crossfades&);

	template<class T> void apply_to_history (T& obj, void (T::*method)(const ARDOUR::StateManager::StateMap&, state_id_t)) {
		RegionLock rlock (this);
		(obj.*method) (states, _current_state_id);
	}

	bool destroy_region (Region*);

	void get_equivalent_regions (const AudioRegion&, std::vector<AudioRegion*>&);
	void get_region_list_equivalent_regions (const AudioRegion&, std::vector<AudioRegion*>&);

	void drop_all_states ();

    protected:

	/* state management */

	StateManager::State* state_factory (std::string) const;
	Change restore_state (StateManager::State&);
	void send_state_change (Change);

	/* playlist "callbacks" */
	void notify_crossfade_added (Crossfade *);
	void flush_notifications ();

		void finalize_split_region (Region *orig, Region *left, Region *right);
	
        void refresh_dependents (Region& region);
        void check_dependents (Region& region, bool norefresh);
        void remove_dependents (Region& region);

    protected:
       ~AudioPlaylist (); /* public should use unref() */

    private:
       Crossfades      _crossfades;
       Crossfades      _pending_xfade_adds;

       void crossfade_invalidated (Crossfade*);
       XMLNode& state (bool full_state);
       void dump () const;

       bool region_changed (Change, Region*);
       void crossfade_changed (Change);
       void add_crossfade (Crossfade&);
};

} /* namespace ARDOUR */

#endif	/* __ardour_audio_playlist_h__ */


