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

#include <algorithm>

#include <stdlib.h>

#include <sigc++/bind.h>

#include <ardour/types.h>
#include <ardour/configuration.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/crossfade.h>
#include <ardour/crossfade_compare.h>
#include <ardour/session.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace sigc;
using namespace std;

AudioPlaylist::State::~State ()
{
}

AudioPlaylist::AudioPlaylist (Session& session, const XMLNode& node, bool hidden)
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

AudioPlaylist::AudioPlaylist (Session& session, string name, bool hidden)
	: Playlist (session, name, hidden)
{
	save_state (_("initial state"));

	if (!hidden) {
		PlaylistCreated (this); /* EMIT SIGNAL */
	}

}

AudioPlaylist::AudioPlaylist (const AudioPlaylist& other, string name, bool hidden)
	: Playlist (other, name, hidden)
{
	save_state (_("initial state"));

	list<Region*>::const_iterator in_o  = other.regions.begin();
	list<Region*>::iterator in_n = regions.begin();

	while (in_o != other.regions.end()) {
		AudioRegion *ar = dynamic_cast<AudioRegion *>( (*in_o) );

		// We look only for crossfades which begin with the current region, so we don't get doubles
		for (list<Crossfade *>::const_iterator xfades = other._crossfades.begin(); xfades != other._crossfades.end(); ++xfades) {
			if ( &(*xfades)->in() == ar) {
				// We found one! Now copy it!

				list<Region*>::const_iterator out_o = other.regions.begin();
				list<Region*>::const_iterator out_n = regions.begin();

				while (out_o != other.regions.end()) {
					
					AudioRegion *ar2 = dynamic_cast<AudioRegion *>( (*out_o) );
					
					if ( &(*xfades)->out() == ar2) {
						AudioRegion *in  = dynamic_cast<AudioRegion*>( (*in_n) );
						AudioRegion *out = dynamic_cast<AudioRegion*>( (*out_n) );
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

	if (!hidden) {
		PlaylistCreated (this); /* EMIT SIGNAL */
	}
}

AudioPlaylist::AudioPlaylist (const AudioPlaylist& other, jack_nframes_t start, jack_nframes_t cnt, string name, bool hidden)
	: Playlist (other, start, cnt, name, hidden)
{
	save_state (_("initial state"));

	/* this constructor does NOT notify others (session) */
}

AudioPlaylist::~AudioPlaylist ()
{
	set<Crossfade*> all_xfades;
	set<Region*> all_regions;

  	GoingAway (this);

	/* find every region we've ever used, and add it to the set of 
	   all regions. same for xfades;
	*/

	for (RegionList::iterator x = regions.begin(); x != regions.end(); ++x) {
		all_regions.insert (*x);
	}

	for (Crossfades::iterator x = _crossfades.begin(); x != _crossfades.end(); ++x) {
		all_xfades.insert (*x);
	}

	for (StateMap::iterator i = states.begin(); i != states.end(); ++i) {
		
		AudioPlaylist::State* apstate = dynamic_cast<AudioPlaylist::State*> (*i);

		for (RegionList::iterator r = apstate->regions.begin(); r != apstate->regions.end(); ++r) {
			all_regions.insert (*r);
		}
		for (Crossfades::iterator xf = apstate->crossfades.begin(); xf != apstate->crossfades.end(); ++xf) {
			all_xfades.insert (*xf);
		}

		delete apstate;
	}

	/* delete every region */

	for (set<Region *>::iterator ar = all_regions.begin(); ar != all_regions.end(); ++ar) {
		(*ar)->unlock_sources ();
		delete *ar;
	}

	/* delete every crossfade */

	for (set<Crossfade *>::iterator axf = all_xfades.begin(); axf != all_xfades.end(); ++axf) {
		delete *axf;
	}
}

struct RegionSortByLayer {
    bool operator() (Region *a, Region *b) {
	    return a->layer() < b->layer();
    }
};

jack_nframes_t
AudioPlaylist::read (Sample *buf, Sample *mixdown_buffer, float *gain_buffer, char * workbuf, jack_nframes_t start,
		     jack_nframes_t cnt, unsigned chan_n)
{
	jack_nframes_t ret = cnt;
	jack_nframes_t end;
	jack_nframes_t read_frames;
	jack_nframes_t skip_frames;

	/* optimizing this memset() away involves a lot of conditionals
	   that may well cause more of a hit due to cache misses 
	   and related stuff than just doing this here.
	   
	   it would be great if someone could measure this
	   at some point.

	   one way or another, parts of the requested area
	   that are not written to by Region::region_at()
	   for all Regions that cover the area need to be
	   zeroed.
	*/

	memset (buf, 0, sizeof (Sample) * cnt);

	/* this function is never called from a realtime thread, so 
	   its OK to block (for short intervals).
	*/

	LockMonitor rm (region_lock, __LINE__, __FILE__);

	end =  start + cnt - 1;

	read_frames = 0;
	skip_frames = 0;
	_read_data_count = 0;

	map<uint32_t,vector<Region*> > relevant_regions;
	map<uint32_t,vector<Crossfade*> > relevant_xfades;
	vector<uint32_t> relevant_layers;

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->coverage (start, end) != OverlapNone) {
			
			relevant_regions[(*i)->layer()].push_back (*i);
			relevant_layers.push_back ((*i)->layer());
		}
	}

	for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		if ((*i)->coverage (start, end) != OverlapNone) {
			relevant_xfades[(*i)->upper_layer()].push_back (*i);
		}
	}

//	RegionSortByLayer layer_cmp;
//	relevant_regions.sort (layer_cmp);

	/* XXX this whole per-layer approach is a hack that
	   should be removed once Crossfades become
	   CrossfadeRegions and we just grab a list of relevant
	   regions and call read_at() on all of them.
	*/

	sort (relevant_layers.begin(), relevant_layers.end());

	for (vector<uint32_t>::iterator l = relevant_layers.begin(); l != relevant_layers.end(); ++l) {

		vector<Region*>& r (relevant_regions[*l]);
		vector<Crossfade*>& x (relevant_xfades[*l]);

		for (vector<Region*>::iterator i = r.begin(); i != r.end(); ++i) {
			(*i)->read_at (buf, mixdown_buffer, gain_buffer, workbuf, start, cnt, chan_n, read_frames, skip_frames);
			_read_data_count += (*i)->read_data_count();
		}
		
		for (vector<Crossfade*>::iterator i = x.begin(); i != x.end(); ++i) {
			
			(*i)->read_at (buf, mixdown_buffer, gain_buffer, workbuf, start, cnt, chan_n);

			/* don't JACK up _read_data_count, since its the same data as we just
			   read from the regions, and the OS should handle that for us.
			*/
		}
	}

	return ret;
}


void
AudioPlaylist::remove_dependents (Region& region)
{
	Crossfades::iterator i, tmp;
	AudioRegion* r = dynamic_cast<AudioRegion*> (&region);
	
	if (r == 0) {
		fatal << _("programming error: non-audio Region passed to remove_overlap in audio playlist")
		      << endmsg;
		return;
	}

	for (i = _crossfades.begin(); i != _crossfades.end(); ) {
		tmp = i;
		tmp++;

		if ((*i)->involves (*r)) {
			/* do not delete crossfades */
			_crossfades.erase (i);
		}
		
		i = tmp;
	}
}


void
AudioPlaylist::flush_notifications ()
{
	Playlist::flush_notifications();

	if (in_flush) {
		return;
	}

	in_flush = true;

	Crossfades::iterator a;
	for (a = _pending_xfade_adds.begin(); a != _pending_xfade_adds.end(); ++a) {
		NewCrossfade (*a); /* EMIT SIGNAL */
	}

	_pending_xfade_adds.clear ();
	
	in_flush = false;
}

void
AudioPlaylist::refresh_dependents (Region& r)
{
	AudioRegion* ar = dynamic_cast<AudioRegion*>(&r);
	set<Crossfade*> updated;

	if (ar == 0) {
		return;
	}

	for (Crossfades::iterator x = _crossfades.begin(); x != _crossfades.end();) {

		Crossfades::iterator tmp;
		
		tmp = x;
		++tmp;

		/* only update them once */

		if ((*x)->involves (*ar)) {

			if (find (updated.begin(), updated.end(), *x) == updated.end()) {
				if ((*x)->refresh ()) {
					/* not invalidated by the refresh */
					updated.insert (*x);
				}
			}
		}

		x = tmp;
	}
}

void
AudioPlaylist::finalize_split_region (Region *o, Region *l, Region *r)
{
	AudioRegion *orig  = dynamic_cast<AudioRegion*>(o);
	AudioRegion *left  = dynamic_cast<AudioRegion*>(l);
	AudioRegion *right = dynamic_cast<AudioRegion*>(r);

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
	}
}

void
AudioPlaylist::check_dependents (Region& r, bool norefresh)
{
	AudioRegion* other;
	AudioRegion* region;
	AudioRegion* top;
	AudioRegion* bottom;
	Crossfade*   xfade;

	if (in_set_state || in_partition) {
		return;
	}

	if ((region = dynamic_cast<AudioRegion*> (&r)) == 0) {
		fatal << _("programming error: non-audio Region tested for overlap in audio playlist")
		      << endmsg;
		return;
	}

	if (!norefresh) {
		refresh_dependents (r);
	}

	if (!Config->get_auto_xfade()) {
		return;
	}

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		other = dynamic_cast<AudioRegion*> (*i);

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

		try {
				
			if (top->coverage (bottom->position(), bottom->last_frame()) != OverlapNone) {
				
				/* check if the upper region is within the lower region */
				
				if (top->first_frame() > bottom->first_frame() &&
				    top->last_frame() < bottom->last_frame()) {
					
					
					/*     [ -------- top ------- ]
					 * {=========== bottom =============}
					 */
					
					/* to avoid discontinuities at the region boundaries of an internal
					   overlap (this region is completely within another), we create
					   two hidden crossfades at each boundary. this is not dependent
					   on the auto-xfade option, because we require it as basic
					   audio engineering.
					*/
					
					jack_nframes_t xfade_length = min ((jack_nframes_t) 720, top->length());
					
					                    /*  in,      out */
					xfade = new Crossfade (*top, *bottom, xfade_length, top->first_frame(), StartOfIn);
					add_crossfade (*xfade);
					xfade = new Crossfade (*bottom, *top, xfade_length, top->last_frame() - xfade_length, EndOfOut);
					add_crossfade (*xfade);
					
				} else {
		
					xfade = new Crossfade (*other, *region, _session.get_xfade_model(), _session.get_crossfades_active());
					add_crossfade (*xfade);
				}
			} 
		}
		
		catch (failed_constructor& err) {
			continue;
		}
		
		catch (Crossfade::NoCrossfadeHere& err) {
			continue;
		}
		
	}
}

void
AudioPlaylist::add_crossfade (Crossfade& xfade)
{
	Crossfades::iterator ci;

	for (ci = _crossfades.begin(); ci != _crossfades.end(); ++ci) {
		if (*(*ci) == xfade) { // Crossfade::operator==()
			break;
		}
	}
	
	if (ci != _crossfades.end()) {
		delete &xfade;
	} else {
		_crossfades.push_back (&xfade);

		xfade.Invalidated.connect (mem_fun (*this, &AudioPlaylist::crossfade_invalidated));
		xfade.StateChanged.connect (mem_fun (*this, &AudioPlaylist::crossfade_changed));

		notify_crossfade_added (&xfade);
	}
}
	
void AudioPlaylist::notify_crossfade_added (Crossfade *x)
{
	if (atomic_read(&block_notifications)) {
		_pending_xfade_adds.insert (_pending_xfade_adds.end(), x);
	} else {
		NewCrossfade (x); /* EMIT SIGNAL */
	}
}

void
AudioPlaylist::crossfade_invalidated (Crossfade* xfade)
{
	Crossfades::iterator i;

	xfade->in().resume_fade_in ();
	xfade->out().resume_fade_out ();

	if ((i = find (_crossfades.begin(), _crossfades.end(), xfade)) != _crossfades.end()) {
		_crossfades.erase (i);
	}
}

int
AudioPlaylist::set_state (const XMLNode& node)
{
	XMLNode *child;
	XMLNodeList nlist;
	XMLNodeConstIterator niter;

	if (!in_set_state) {
		Playlist::set_state (node);
	}

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		child = *niter;

		if (child->name() == "Crossfade") {

			Crossfade *xfade;
			
			try {
				xfade = new Crossfade (*((const Playlist *)this), *child);
			}

			catch (failed_constructor& err) {
			  //	cout << string_compose (_("could not create crossfade object in playlist %1"),
			  //	  _name) 
			  //    << endl;
				continue;
			}

			Crossfades::iterator ci;

			for (ci = _crossfades.begin(); ci != _crossfades.end(); ++ci) {
				if (*(*ci) == *xfade) {
					break;
				}
			}

			if (ci == _crossfades.end()) {
				_crossfades.push_back (xfade);
				xfade->Invalidated.connect (mem_fun (*this, &AudioPlaylist::crossfade_invalidated));
				xfade->StateChanged.connect (mem_fun (*this, &AudioPlaylist::crossfade_changed));
				/* no need to notify here */
			} else {
				delete xfade;
			}
		}

	}

	return 0;
}

void
AudioPlaylist::drop_all_states ()
{
	set<Crossfade*> all_xfades;
	set<Region*> all_regions;

	/* find every region we've ever used, and add it to the set of 
	   all regions. same for xfades;
	*/

	for (StateMap::iterator i = states.begin(); i != states.end(); ++i) {
		
		AudioPlaylist::State* apstate = dynamic_cast<AudioPlaylist::State*> (*i);

		for (RegionList::iterator r = apstate->regions.begin(); r != apstate->regions.end(); ++r) {
			all_regions.insert (*r);
		}
		for (Crossfades::iterator xf = apstate->crossfades.begin(); xf != apstate->crossfades.end(); ++xf) {
			all_xfades.insert (*xf);
		}
	}

	/* now remove from the "all" lists every region that is in the current list. */

	for (list<Region*>::iterator i = regions.begin(); i != regions.end(); ++i) {
		set<Region*>::iterator x = all_regions.find (*i);
		if (x != all_regions.end()) {
			all_regions.erase (x);
		}
	}

	/* ditto for every crossfade */

	for (list<Crossfade*>::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		set<Crossfade*>::iterator x = all_xfades.find (*i);
		if (x != all_xfades.end()) {
			all_xfades.erase (x);
		}
	}

	/* delete every region that is left - these are all things that are part of our "history" */

	for (set<Region *>::iterator ar = all_regions.begin(); ar != all_regions.end(); ++ar) {
		(*ar)->unlock_sources ();
		delete *ar;
	}

	/* delete every crossfade that is left (ditto as per regions) */

	for (set<Crossfade *>::iterator axf = all_xfades.begin(); axf != all_xfades.end(); ++axf) {
		delete *axf;
	}

	/* Now do the generic thing ... */

	StateManager::drop_all_states ();
}

StateManager::State*
AudioPlaylist::state_factory (std::string why) const
{
	State* state = new State (why);

	state->regions = regions;
	state->region_states.clear ();
	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		state->region_states.push_back ((*i)->get_memento());
	}

	state->crossfades = _crossfades;
	state->crossfade_states.clear ();
	for (Crossfades::const_iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		state->crossfade_states.push_back ((*i)->get_memento());
	}
	return state;
}

Change
AudioPlaylist::restore_state (StateManager::State& state)
{ 
	{ 
		RegionLock rlock (this);
		State* apstate = dynamic_cast<State*> (&state);

		in_set_state = true;

		regions = apstate->regions;

		for (list<UndoAction>::iterator s = apstate->region_states.begin(); s != apstate->region_states.end(); ++s) {
			(*s) ();
		}

		_crossfades = apstate->crossfades;
		
		for (list<UndoAction>::iterator s = apstate->crossfade_states.begin(); s != apstate->crossfade_states.end(); ++s) {
			(*s) ();
		}

		in_set_state = false;
	}

	notify_length_changed ();
	return Change (~0);
}

UndoAction
AudioPlaylist::get_memento () const
{
	return sigc::bind (mem_fun (*(const_cast<AudioPlaylist*> (this)), &StateManager::use_state), _current_state_id);
}

void
AudioPlaylist::clear (bool with_delete, bool with_save)
{
	if (with_delete) {
		for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
			delete *i;
		}
	}

	_crossfades.clear ();
	
	Playlist::clear (with_delete, with_save);
}

XMLNode&
AudioPlaylist::state (bool full_state)
{
	XMLNode& node = Playlist::state (full_state);

	if (full_state) {
		for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
			node.add_child_nocopy ((*i)->get_state());
		}
	}
	
	return node;
}

void
AudioPlaylist::dump () const
{
	Region *r;
	Crossfade *x;

	cerr << "Playlist \"" << _name << "\" " << endl
	     << regions.size() << " regions "
	     << _crossfades.size() << " crossfades"
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

	for (Crossfades::const_iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		x = *i;
		cerr << "  xfade [" 
		     << x->out().name()
		     << ','
		     << x->in().name()
		     << " @ "
		     << x->position()
		     << " length = " 
		     << x->length ()
		     << " active ? "
		     << (x->active() ? "yes" : "no")
		     << endl;
	}
}

bool
AudioPlaylist::destroy_region (Region* region)
{
	AudioRegion* r = dynamic_cast<AudioRegion*> (region);
	bool changed = false;
	Crossfades::iterator c, ctmp;
	set<Crossfade*> unique_xfades;

	if (r == 0) {
		fatal << _("programming error: non-audio Region passed to remove_overlap in audio playlist")
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

	for (c = _crossfades.begin(); c != _crossfades.end(); ) {
		ctmp = c;
		++ctmp;

		if ((*c)->involves (*r)) {
			unique_xfades.insert (*c);
			_crossfades.erase (c);
		}
		
		c = ctmp;
	}

	for (StateMap::iterator s = states.begin(); s != states.end(); ) {
		StateMap::iterator tmp;

		tmp = s;
		++tmp;

		State* astate = dynamic_cast<State*> (*s);
		
		for (c = astate->crossfades.begin(); c != astate->crossfades.end(); ) {

			ctmp = c;
			++ctmp;

			if ((*c)->involves (*r)) {
				unique_xfades.insert (*c);
				_crossfades.erase (c);
			}

			c = ctmp;
		}

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

	for (set<Crossfade*>::iterator c = unique_xfades.begin(); c != unique_xfades.end(); ++c) {
		delete *c;
	}

	if (changed) {
		/* overload this, it normally means "removed", not destroyed */
		notify_region_removed (region);
	}

	return changed;
}

void
AudioPlaylist::crossfade_changed (Change ignored)
{
	if (in_flush || in_set_state) {
		return;
	}

	/* XXX is there a loop here? can an xfade change not happen
	   due to a playlist change? well, sure activation would
	   be an example. maybe we should check the type of change
	   that occured.
	*/

	maybe_save_state (_("xfade change"));

	notify_modified ();
}

void
AudioPlaylist::get_equivalent_regions (const AudioRegion& other, vector<AudioRegion*>& results)
{
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		AudioRegion* ar = dynamic_cast<AudioRegion*> (*i);

		if (ar && ar->equivalent (other)) {
			results.push_back (ar);
		}
	}
}

void
AudioPlaylist::get_region_list_equivalent_regions (const AudioRegion& other, vector<AudioRegion*>& results)
{
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		AudioRegion* ar = dynamic_cast<AudioRegion*> (*i);
		
		if (ar && ar->region_list_equivalent (other)) {
			results.push_back (ar);
		}
	}
}

bool
AudioPlaylist::region_changed (Change what_changed, Region* region)
{
	if (in_flush || in_set_state) {
		return false;
	}

	Change our_interests = Change (AudioRegion::FadeInChanged|
				       AudioRegion::FadeOutChanged|
				       AudioRegion::FadeInActiveChanged|
				       AudioRegion::FadeOutActiveChanged|
				       AudioRegion::EnvelopeActiveChanged|
				       AudioRegion::ScaleAmplitudeChanged|
				       AudioRegion::EnvelopeChanged);
	bool parent_wants_notify;

	parent_wants_notify = Playlist::region_changed (what_changed, region);

	maybe_save_state (_("region modified"));

	if ((parent_wants_notify || (what_changed & our_interests))) {
		notify_modified ();
	}

	return true; 
}

void
AudioPlaylist::crossfades_at (jack_nframes_t frame, Crossfades& clist)
{
	RegionLock rlock (this);

	for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		jack_nframes_t start, end;

		start = (*i)->position();
		end = start + (*i)->overlap_length(); // not length(), important difference

		if (frame >= start && frame <= end) {
			clist.push_back (*i);
		} 
	}
}
