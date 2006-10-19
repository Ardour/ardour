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

#include <cstdlib>

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
using namespace PBD;

AudioPlaylist::AudioPlaylist (Session& session, const XMLNode& node, bool hidden)
	: Playlist (session, node, hidden)
{
	in_set_state = true;
	set_state (node);
	in_set_state = false;

	if (!hidden) {
		PlaylistCreated (this); /* EMIT SIGNAL */
	}
}

AudioPlaylist::AudioPlaylist (Session& session, string name, bool hidden)
	: Playlist (session, name, hidden)
{
	if (!hidden) {
		PlaylistCreated (this); /* EMIT SIGNAL */
	}

}

AudioPlaylist::AudioPlaylist (const AudioPlaylist& other, string name, bool hidden)
	: Playlist (other, name, hidden)
{
	RegionList::const_iterator in_o  = other.regions.begin();
	RegionList::iterator in_n = regions.begin();

	while (in_o != other.regions.end()) {
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(*in_o);

		// We look only for crossfades which begin with the current region, so we don't get doubles
		for (list<Crossfade *>::const_iterator xfades = other._crossfades.begin(); xfades != other._crossfades.end(); ++xfades) {
			if ((*xfades)->in() == ar) {
				// We found one! Now copy it!

				RegionList::const_iterator out_o = other.regions.begin();
				RegionList::const_iterator out_n = regions.begin();

				while (out_o != other.regions.end()) {
					
					boost::shared_ptr<AudioRegion>ar2 = boost::dynamic_pointer_cast<AudioRegion>(*out_o);
					
					if ((*xfades)->out() == ar2) {
						boost::shared_ptr<AudioRegion>in  = boost::dynamic_pointer_cast<AudioRegion>(*in_n);
						boost::shared_ptr<AudioRegion>out = boost::dynamic_pointer_cast<AudioRegion>(*out_n);
						Crossfade *new_fade = new Crossfade (*(*xfades), in, out);
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

AudioPlaylist::AudioPlaylist (const AudioPlaylist& other, nframes_t start, nframes_t cnt, string name, bool hidden)
	: Playlist (other, start, cnt, name, hidden)
{
	/* this constructor does NOT notify others (session) */
}

AudioPlaylist::~AudioPlaylist ()
{
	set<Crossfade*> all_xfades;

  	GoingAway (); /* EMIT SIGNAL */

	/* drop connections to signals */

	notify_callbacks ();

	for (Crossfades::iterator x = _crossfades.begin(); x != _crossfades.end(); ++x) {
		delete *x;
	}
}

struct RegionSortByLayer {
    bool operator() (boost::shared_ptr<Region>a, boost::shared_ptr<Region>b) {
	    return a->layer() < b->layer();
    }
};

nframes_t
AudioPlaylist::read (Sample *buf, Sample *mixdown_buffer, float *gain_buffer, nframes_t start,
		     nframes_t cnt, unsigned chan_n)
{
	nframes_t ret = cnt;
	nframes_t end;
	nframes_t read_frames;
	nframes_t skip_frames;

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

	Glib::Mutex::Lock rm (region_lock);

	end =  start + cnt - 1;

	read_frames = 0;
	skip_frames = 0;
	_read_data_count = 0;

	map<uint32_t,vector<boost::shared_ptr<Region> > > relevant_regions;
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

		vector<boost::shared_ptr<Region> > r (relevant_regions[*l]);
		vector<Crossfade*>& x (relevant_xfades[*l]);

		for (vector<boost::shared_ptr<Region> >::iterator i = r.begin(); i != r.end(); ++i) {
			boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(*i);
			assert(ar);
			ar->read_at (buf, mixdown_buffer, gain_buffer, start, cnt, chan_n, read_frames, skip_frames);
			_read_data_count += ar->read_data_count();
		}
		
		for (vector<Crossfade*>::iterator i = x.begin(); i != x.end(); ++i) {
			(*i)->read_at (buf, mixdown_buffer, gain_buffer, start, cnt, chan_n);

			/* don't JACK up _read_data_count, since its the same data as we just
			   read from the regions, and the OS should handle that for us.
			*/
		}
	}

	return ret;
}


void
AudioPlaylist::remove_dependents (boost::shared_ptr<Region> region)
{
	Crossfades::iterator i, tmp;
	boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion> (region);
	
	if (r == 0) {
		fatal << _("programming error: non-audio Region passed to remove_overlap in audio playlist")
		      << endmsg;
		return;
	}

	for (i = _crossfades.begin(); i != _crossfades.end(); ) {
		tmp = i;
		tmp++;

		if ((*i)->involves (r)) {
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
AudioPlaylist::refresh_dependents (boost::shared_ptr<Region> r)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(r);
	set<Crossfade*> updated;

	if (ar == 0) {
		return;
	}

	for (Crossfades::iterator x = _crossfades.begin(); x != _crossfades.end();) {

		Crossfades::iterator tmp;
		
		tmp = x;
		++tmp;

		/* only update them once */

		if ((*x)->involves (ar)) {

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
AudioPlaylist::finalize_split_region (boost::shared_ptr<Region> o, boost::shared_ptr<Region> l, boost::shared_ptr<Region> r)
{
	boost::shared_ptr<AudioRegion> orig  = boost::dynamic_pointer_cast<AudioRegion>(o);
	boost::shared_ptr<AudioRegion> left  = boost::dynamic_pointer_cast<AudioRegion>(l);
	boost::shared_ptr<AudioRegion> right = boost::dynamic_pointer_cast<AudioRegion>(r);

	for (Crossfades::iterator x = _crossfades.begin(); x != _crossfades.end();) {
		Crossfades::iterator tmp;
		tmp = x;
		++tmp;

		Crossfade *fade = 0;
		
		if ((*x)->_in == orig) {
			if (! (*x)->covers(right->position())) {
				fade = new Crossfade (**x, left, (*x)->_out);
			} else {
				// Overlap, the crossfade is copied on the left side of the right region instead
				fade = new Crossfade (**x, right, (*x)->_out);
			}
		}
		
		if ((*x)->_out == orig) {
			if (! (*x)->covers(right->position())) {
				fade = new Crossfade (**x, (*x)->_in, right);
			} else {
				// Overlap, the crossfade is copied on the right side of the left region instead
				fade = new Crossfade (**x, (*x)->_in, left);
			}
		}
		
		if (fade) {
			_crossfades.remove (*x);
			add_crossfade (*fade);
		}
		x = tmp;
	}
}

void
AudioPlaylist::check_dependents (boost::shared_ptr<Region> r, bool norefresh)
{
	boost::shared_ptr<AudioRegion> other;
	boost::shared_ptr<AudioRegion> region;
	boost::shared_ptr<AudioRegion> top;
	boost::shared_ptr<AudioRegion> bottom;
	Crossfade*   xfade;

	if (in_set_state || in_partition) {
		return;
	}

	if ((region = boost::dynamic_pointer_cast<AudioRegion> (r)) == 0) {
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

		other = boost::dynamic_pointer_cast<AudioRegion> (*i);

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
					
					nframes_t xfade_length = min ((nframes_t) 720, top->length());
					
					                    /*  in,      out */
					xfade = new Crossfade (top, bottom, xfade_length, top->first_frame(), StartOfIn);
					add_crossfade (*xfade);
					xfade = new Crossfade (bottom, top, xfade_length, top->last_frame() - xfade_length, EndOfOut);
					add_crossfade (*xfade);
					
				} else {

					xfade = new Crossfade (other, region, Config->get_xfade_model(), Config->get_crossfades_active());
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
	if (g_atomic_int_get(&block_notifications)) {
		_pending_xfade_adds.insert (_pending_xfade_adds.end(), x);
	} else {
		NewCrossfade (x); /* EMIT SIGNAL */
	}
}

void
AudioPlaylist::crossfade_invalidated (Crossfade* xfade)
{
	Crossfades::iterator i;

	xfade->in()->resume_fade_in ();
	xfade->out()->resume_fade_out ();

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

		if (child->name() != "Crossfade") {
			continue;
		}

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
			NewCrossfade(xfade);
		} else {

			/* adjust the current state of the existing crossfade */

			(*ci)->set_state (*child);

			/* drop the new one */
			delete xfade;
		}
	}

	return 0;
}

void
AudioPlaylist::clear ()
{
	for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		delete *i;
	}

	_crossfades.clear ();
	
	Playlist::clear ();
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
	boost::shared_ptr<Region>r;
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
		     << x->out()->name()
		     << ','
		     << x->in()->name()
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
AudioPlaylist::destroy_region (boost::shared_ptr<Region> region)
{
	boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion> (region);
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
				regions.erase (i);
				changed = true;
			}
			
			i = tmp;
		}
	}

	for (c = _crossfades.begin(); c != _crossfades.end(); ) {
		ctmp = c;
		++ctmp;

		if ((*c)->involves (r)) {
			unique_xfades.insert (*c);
			_crossfades.erase (c);
		}
		
		c = ctmp;
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

	notify_modified ();
}

bool
AudioPlaylist::region_changed (Change what_changed, boost::shared_ptr<Region> region)
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

	if ((parent_wants_notify || (what_changed & our_interests))) {
		notify_modified ();
	}

	return true; 
}

void
AudioPlaylist::crossfades_at (nframes_t frame, Crossfades& clist)
{
	RegionLock rlock (this);

	for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		nframes_t start, end;

		start = (*i)->position();
		end = start + (*i)->overlap_length(); // not length(), important difference

		if (frame >= start && frame <= end) {
			clist.push_back (*i);
		} 
	}
}

