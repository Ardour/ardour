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

#include <cassert>

#include <algorithm>

#include <stdlib.h>

#include <sigc++/bind.h>

#include <ardour/types.h>
#include <ardour/configuration.h>
#include <ardour/midi_playlist.h>
#include <ardour/midi_region.h>
#include <ardour/session.h>

#include <pbd/error.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace sigc;
using namespace std;

MidiPlaylist::State::~State ()
{}

MidiPlaylist::MidiPlaylist (Session& session, const XMLNode& node, bool hidden)
		: Playlist (session, node, hidden)
{
	in_set_state = true;
	set_state (node);
	in_set_state = false;

	save_state (_("initial state"));

	if (!hidden) {
		PlaylistCreated (this); /* EMIT SIGNAL */
	}
}

MidiPlaylist::MidiPlaylist (Session& session, string name, bool hidden)
		: Playlist (session, name, hidden)
{
	save_state (_("initial state"));

	if (!hidden) {
		PlaylistCreated (this); /* EMIT SIGNAL */
	}

}

MidiPlaylist::MidiPlaylist (const MidiPlaylist& other, string name, bool hidden)
		: Playlist (other, name, hidden)
{
	save_state (_("initial state"));

	/*
	list<Region*>::const_iterator in_o  = other.regions.begin();
	list<Region*>::iterator in_n = regions.begin();

	while (in_o != other.regions.end()) {
		MidiRegion *ar = dynamic_cast<MidiRegion *>( (*in_o) );

		for (list<Crossfade *>::const_iterator xfades = other._crossfades.begin(); xfades != other._crossfades.end(); ++xfades) {
			if ( &(*xfades)->in() == ar) {
				// We found one! Now copy it!

				list<Region*>::const_iterator out_o = other.regions.begin();
				list<Region*>::const_iterator out_n = regions.begin();

				while (out_o != other.regions.end()) {

					MidiRegion *ar2 = dynamic_cast<MidiRegion *>( (*out_o) );

					if ( &(*xfades)->out() == ar2) {
						MidiRegion *in  = dynamic_cast<MidiRegion*>( (*in_n) );
						MidiRegion *out = dynamic_cast<MidiRegion*>( (*out_n) );
						Crossfade *new_fade = new Crossfade( *(*xfades), in, out);
						add_crossfade(*new_fade);
						break;
					}

					out_o++;
					out_n++;
				}
				//				cerr << "HUH!? second region in the crossfade not found!" << endl;
			}
		}

		in_o++;
		in_n++;
	}
*/
	if (!hidden) {
		PlaylistCreated (this); /* EMIT SIGNAL */
	}
}

MidiPlaylist::MidiPlaylist (const MidiPlaylist& other, jack_nframes_t start, jack_nframes_t cnt, string name, bool hidden)
		: Playlist (other, start, cnt, name, hidden)
{
	save_state (_("initial state"));

	/* this constructor does NOT notify others (session) */
}

MidiPlaylist::~MidiPlaylist ()
{
	set <Region*> all_regions;

	GoingAway (this);

	/* find every region we've ever used, and add it to the set of
	   all regions.
	*/

	for (RegionList::iterator x = regions.begin(); x != regions.end(); ++x) {
		all_regions.insert (*x);
	}

	for (StateMap::iterator i = states.begin(); i != states.end(); ++i) {

		MidiPlaylist::State* apstate = dynamic_cast<MidiPlaylist::State*> (*i);

		for (RegionList::iterator r = apstate->regions.begin(); r != apstate->regions.end(); ++r) {
			all_regions.insert (*r);
		}

		delete apstate;
	}

	/* delete every region */

	for (set<Region *>::iterator ar = all_regions.begin(); ar != all_regions.end(); ++ar) {
		(*ar)->unlock_sources ();
		delete *ar;
	}

}

struct RegionSortByLayer
{
	bool operator() (Region *a, Region *b)
	{
		return a->layer() < b->layer();
	}
};

/** FIXME: semantics of return value? */
jack_nframes_t
MidiPlaylist::read (RawMidi *buf, RawMidi *mixdown_buffer, jack_nframes_t start,
                     jack_nframes_t cnt, unsigned chan_n)
{
	/* this function is never called from a realtime thread, so
	   its OK to block (for short intervals).
	*/

	Glib::Mutex::Lock rm (region_lock);

	jack_nframes_t ret         = 0;
	jack_nframes_t end         =  start + cnt - 1;
	jack_nframes_t read_frames = 0;
	jack_nframes_t skip_frames = 0;

	_read_data_count = 0;

	vector<MidiRegion*> regs; // relevent regions overlapping start <--> end

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		MidiRegion* const mr = dynamic_cast<MidiRegion*>(*i);
		if (mr && mr->coverage (start, end) != OverlapNone) {
			regs.push_back(mr);
		}
	}

	RegionSortByLayer layer_cmp;
	sort(regs.begin(), regs.end(), layer_cmp);

	for (vector<MidiRegion*>::iterator i = regs.begin(); i != regs.end(); ++i) {
			(*i)->read_at (buf, mixdown_buffer, start, cnt, chan_n, read_frames, skip_frames);
			ret += (*i)->read_data_count();
	}

	_read_data_count += ret;
	
	return ret;
}


void
MidiPlaylist::remove_dependents (Region& region)
{
	MidiRegion* r = dynamic_cast<MidiRegion*> (&region);

	if (r == 0) {
		PBD::fatal << _("programming error: non-midi Region passed to remove_overlap in midi playlist")
		<< endmsg;
		return;
	}

}


void
MidiPlaylist::flush_notifications ()
{
	Playlist::flush_notifications();

	if (in_flush) {
		return;
	}

	in_flush = true;

	in_flush = false;
}

void
MidiPlaylist::refresh_dependents (Region& r)
{
	MidiRegion* ar = dynamic_cast<MidiRegion*>(&r);

	if (ar == 0) {
		return;
	}
}

void
MidiPlaylist::finalize_split_region (Region *o, Region *l, Region *r)
{
	/*
	MidiRegion *orig  = dynamic_cast<MidiRegion*>(o);
	MidiRegion *left  = dynamic_cast<MidiRegion*>(l);
	MidiRegion *right = dynamic_cast<MidiRegion*>(r);

	for (Crossfades::iterator x = _crossfades.begin(); x != _crossfades.end();) {
		Crossfades::iterator tmp;
		tmp = x;
		++tmp;

		Crossfade *fade = 0;

		if ((*x)->_in == orig) {
			if (! (*x)->covers(right->position())) {
				fade = new Crossfade( *(*x), left, (*x)->_out);
			} else {
				// Overlap, the crossfade is copied on the left side of the right region instead
				fade = new Crossfade( *(*x), right, (*x)->_out);
			}
		}

		if ((*x)->_out == orig) {
			if (! (*x)->covers(right->position())) {
				fade = new Crossfade( *(*x), (*x)->_in, right);
			} else {
				// Overlap, the crossfade is copied on the right side of the left region instead
				fade = new Crossfade( *(*x), (*x)->_in, left);
			}
		}

		if (fade) {
			_crossfades.remove( (*x) );
			add_crossfade (*fade);
		}
		x = tmp;
	}*/
}

void
MidiPlaylist::check_dependents (Region& r, bool norefresh)
{
	MidiRegion* other;
	MidiRegion* region;
	MidiRegion* top;
	MidiRegion* bottom;

	if (in_set_state || in_partition) {
		return;
	}

	if ((region = dynamic_cast<MidiRegion*> (&r)) == 0) {
		PBD::fatal << _("programming error: non-midi Region tested for overlap in midi playlist")
		<< endmsg;
		return;
	}

	if (!norefresh) {
		refresh_dependents (r);
	}

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		other = dynamic_cast<MidiRegion*> (*i);

		if (other == region) {
			continue;
		}

		if (other->muted() || region->muted()) {
			continue;
		}

		if (other->layer() < region->layer()) {
			top = region;
			bottom = other;
		} else {
			top = other;
			bottom = region;
		}

	}
}


int
MidiPlaylist::set_state (const XMLNode& node)
{
	/*
	XMLNode *child;
	XMLNodeList nlist;
	XMLNodeConstIterator niter;

	if (!in_set_state) {
		Playlist::set_state (node);
	}

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		child = *niter;

	}*/

	return 0;
}

void
MidiPlaylist::drop_all_states ()
{
	set<Region*> all_regions;

	/* find every region we've ever used, and add it to the set of
	   all regions. same for xfades;
	*/

	for (StateMap::iterator i = states.begin(); i != states.end(); ++i) {

		MidiPlaylist::State* apstate = dynamic_cast<MidiPlaylist::State*> (*i);

		for (RegionList::iterator r = apstate->regions.begin(); r != apstate->regions.end(); ++r) {
			all_regions.insert (*r);
		}
	}

	/* now remove from the "all" lists every region that is in the current list. */

	for (list<Region*>::iterator i = regions.begin(); i != regions.end(); ++i) {
		set
			<Region*>::iterator x = all_regions.find (*i);
		if (x != all_regions.end()) {
			all_regions.erase (x);
		}
	}

	/* delete every region that is left - these are all things that are part of our "history" */

	for (set
	        <Region *>::iterator ar = all_regions.begin(); ar != all_regions.end(); ++ar) {
		(*ar)->unlock_sources ();
		delete *ar;
	}

	/* Now do the generic thing ... */

	StateManager::drop_all_states ();
}

StateManager::State*
MidiPlaylist::state_factory (std::string why) const
{
	State* state = new State (why);

	state->regions = regions;
	state->region_states.clear ();
	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		state->region_states.push_back ((*i)->get_memento());
	}

	return state;
}

Change
MidiPlaylist::restore_state (StateManager::State& state)
{
	{
		RegionLock rlock (this);
		State* apstate = dynamic_cast<State*> (&state);

		in_set_state = true;

		regions = apstate->regions;

		for (list<UndoAction>::iterator s = apstate->
		                                    region_states.begin();
		        s != apstate->region_states.end();
		        ++s) {
			(*s) ();
		}

		in_set_state = false;
	}

	notify_length_changed ();
	return Change (~0);
}

UndoAction
MidiPlaylist::get_memento () const
{
	return sigc::bind (mem_fun (*(const_cast<MidiPlaylist*> (this)), &StateManager::use_state), _current_state_id);
}


XMLNode&
MidiPlaylist::state (bool full_state)
{
	XMLNode& node = Playlist::state (full_state);

	return node;
}

void
MidiPlaylist::dump () const
{
	Region *r;

	cerr << "Playlist \"" << _name << "\" " << endl
	<< regions.size() << " regions "
	<< endl;

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		r = *i;
		cerr << "  " << r->name() << " @ " << r << " ["
		<< r->start() << "+" << r->length()
		<< "] at "
		<< r->position()
		<< " on layer "
		<< r->layer ()
		<< endl;
	}
}

bool
MidiPlaylist::destroy_region (Region* region)
{
	MidiRegion* r = dynamic_cast<MidiRegion*> (region);
	bool changed = false;

	if (r == 0) {
		PBD::fatal << _("programming error: non-midi Region passed to remove_overlap in midi playlist")
		<< endmsg;
		/*NOTREACHED*/
		return false;
	}

	{
		RegionLock rlock (this);
		RegionList::iterator i;
		RegionList::iterator tmp;

		for (i = regions.begin(); i != regions.end(); ) {

			tmp = i;
			++tmp;

			if ((*i) == region) {
				(*i)->unlock_sources ();
				regions.erase (i);
				changed = true;
			}

			i = tmp;
		}
	}

	for (StateMap::iterator s = states.begin(); s != states.end(); ) {
		StateMap::iterator tmp;

		tmp = s;
		++tmp;

		State* astate = dynamic_cast<State*> (*s);

		list<UndoAction>::iterator rsi, rsitmp;
		RegionList::iterator ri, ritmp;

		for (ri = astate->regions.begin(), rsi = astate->region_states.begin();
		        ri != astate->regions.end() && rsi != astate->region_states.end();) {


			ritmp = ri;
			++ritmp;

			rsitmp = rsi;
			++rsitmp;

			if (region == (*ri)) {
				astate->regions.erase (ri);
				astate->region_states.erase (rsi);
			}

			ri = ritmp;
			rsi = rsitmp;
		}

		s = tmp;
	}


	if (changed) {
		/* overload this, it normally means "removed", not destroyed */
		notify_region_removed (region);
	}

	return changed;
}


void
MidiPlaylist::get_equivalent_regions (const MidiRegion& other, vector<MidiRegion*>& results)
{
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		MidiRegion* ar = dynamic_cast<MidiRegion*> (*i);

		if (ar) {
			if (Config->get_use_overlap_equivalency()) {
				if (ar->overlap_equivalent (other)) {
					results.push_back (ar);
				} else if (ar->equivalent (other)) {
					results.push_back (ar);
				}
			}
		}
	}
}

void
MidiPlaylist::get_region_list_equivalent_regions (const MidiRegion& other, vector<MidiRegion*>& results)
{
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		MidiRegion* ar = dynamic_cast<MidiRegion*> (*i);

		if (ar && ar->region_list_equivalent (other)) {
			results.push_back (ar);
		}
	}
}

bool
MidiPlaylist::region_changed (Change what_changed, Region* region)
{
	if (in_flush || in_set_state) {
		return false;
	}

	Change our_interests = Change (/*MidiRegion::FadeInChanged|
	                               MidiRegion::FadeOutChanged|
	                               MidiRegion::FadeInActiveChanged|
	                               MidiRegion::FadeOutActiveChanged|
	                               MidiRegion::EnvelopeActiveChanged|
	                               MidiRegion::ScaleAmplitudeChanged|
	                               MidiRegion::EnvelopeChanged*/);
	bool parent_wants_notify;

	parent_wants_notify = Playlist::region_changed (what_changed, region);

	maybe_save_state (_("region modified"));

	if ((parent_wants_notify || (what_changed & our_interests))) {
		notify_modified ();
	}

	return true;
}

