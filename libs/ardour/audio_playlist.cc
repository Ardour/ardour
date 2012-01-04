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

*/

#include <algorithm>

#include <cstdlib>

#include "ardour/types.h"
#include "ardour/debug.h"
#include "ardour/configuration.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/crossfade.h"
#include "ardour/region_sorters.h"
#include "ardour/session.h"
#include "pbd/enumwriter.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool> crossfades;
	}
}

void
AudioPlaylist::make_property_quarks ()
{
        Properties::crossfades.property_id = g_quark_from_static_string (X_("crossfades"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for crossfades = %1\n", Properties::crossfades.property_id));
}

CrossfadeListProperty::CrossfadeListProperty (AudioPlaylist& pl)
        : SequenceProperty<std::list<boost::shared_ptr<Crossfade> > > (Properties::crossfades.property_id, boost::bind (&AudioPlaylist::update, &pl, _1))
        , _playlist (pl)
{

}

CrossfadeListProperty::CrossfadeListProperty (CrossfadeListProperty const & p)
	: PBD::SequenceProperty<std::list<boost::shared_ptr<Crossfade> > > (p)
	, _playlist (p._playlist)
{

}


CrossfadeListProperty *
CrossfadeListProperty::create () const
{
	return new CrossfadeListProperty (_playlist);
}

CrossfadeListProperty *
CrossfadeListProperty::clone () const
{
	return new CrossfadeListProperty (*this);
}

void
CrossfadeListProperty::get_content_as_xml (boost::shared_ptr<Crossfade> xfade, XMLNode & node) const
{
	/* Crossfades are not written to any state when they are no
	   longer in use, so we must write their state here.
	*/

	XMLNode& c = xfade->get_state ();
	node.add_child_nocopy (c);
}

boost::shared_ptr<Crossfade>
CrossfadeListProperty::get_content_from_xml (XMLNode const & node) const
{
	XMLNodeList const c = node.children ();
	assert (c.size() == 1);
	return boost::shared_ptr<Crossfade> (new Crossfade (_playlist, *c.front()));
}


AudioPlaylist::AudioPlaylist (Session& session, const XMLNode& node, bool hidden)
	: Playlist (session, node, DataType::AUDIO, hidden)
	, _crossfades (*this)
{
#ifndef NDEBUG
	const XMLProperty* prop = node.property("type");
	assert(!prop || DataType(prop->value()) == DataType::AUDIO);
#endif

	add_property (_crossfades);

	in_set_state++;
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor();
	}
	in_set_state--;

	relayer ();
}

AudioPlaylist::AudioPlaylist (Session& session, string name, bool hidden)
	: Playlist (session, name, DataType::AUDIO, hidden)
	, _crossfades (*this)
{
	add_property (_crossfades);
}

AudioPlaylist::AudioPlaylist (boost::shared_ptr<const AudioPlaylist> other, string name, bool hidden)
	: Playlist (other, name, hidden)
	, _crossfades (*this)
{
	add_property (_crossfades);

	RegionList::const_iterator in_o  = other->regions.begin();
	RegionList::iterator in_n = regions.begin();

	while (in_o != other->regions.end()) {
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(*in_o);

		// We look only for crossfades which begin with the current region, so we don't get doubles
		for (Crossfades::const_iterator xfades = other->_crossfades.begin(); xfades != other->_crossfades.end(); ++xfades) {
			if ((*xfades)->in() == ar) {
				// We found one! Now copy it!

				RegionList::const_iterator out_o = other->regions.begin();
				RegionList::const_iterator out_n = regions.begin();

				while (out_o != other->regions.end()) {

					boost::shared_ptr<AudioRegion>ar2 = boost::dynamic_pointer_cast<AudioRegion>(*out_o);

					if ((*xfades)->out() == ar2) {
						boost::shared_ptr<AudioRegion>in  = boost::dynamic_pointer_cast<AudioRegion>(*in_n);
						boost::shared_ptr<AudioRegion>out = boost::dynamic_pointer_cast<AudioRegion>(*out_n);
						boost::shared_ptr<Crossfade> new_fade = boost::shared_ptr<Crossfade> (new Crossfade (*xfades, in, out));
						add_crossfade(new_fade);
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
}

AudioPlaylist::AudioPlaylist (boost::shared_ptr<const AudioPlaylist> other, framepos_t start, framecnt_t cnt, string name, bool hidden)
	: Playlist (other, start, cnt, name, hidden)
	, _crossfades (*this)
{
	RegionLock rlock2 (const_cast<AudioPlaylist*> (other.get()));
	in_set_state++;

	add_property (_crossfades);

	framepos_t const end = start + cnt - 1;

	/* Audio regions that have been created by the Playlist constructor
	   will currently have the same fade in/out as the regions that they
	   were created from.  This is wrong, so reset the fades here.
	*/

	RegionList::iterator ours = regions.begin ();

	for (RegionList::const_iterator i = other->regions.begin(); i != other->regions.end(); ++i) {
		boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (*i);
		assert (region);

		framecnt_t fade_in = 64;
		framecnt_t fade_out = 64;

		switch (region->coverage (start, end)) {
		case OverlapNone:
			continue;

		case OverlapInternal:
		{
			framecnt_t const offset = start - region->position ();
			framecnt_t const trim = region->last_frame() - end;
			if (region->fade_in()->back()->when > offset) {
				fade_in = region->fade_in()->back()->when - offset;
			}
			if (region->fade_out()->back()->when > trim) {
				fade_out = region->fade_out()->back()->when - trim;
			}
			break;
		}

		case OverlapStart: {
			if (end > region->position() + region->fade_in()->back()->when)
				fade_in = region->fade_in()->back()->when;  //end is after fade-in, preserve the fade-in
			if (end > region->last_frame() - region->fade_out()->back()->when)
				fade_out = region->fade_out()->back()->when - ( region->last_frame() - end );  //end is inside the fadeout, preserve the fades endpoint
			break;
		}

		case OverlapEnd: {
			if (start < region->last_frame() - region->fade_out()->back()->when)  //start is before fade-out, preserve the fadeout
				fade_out = region->fade_out()->back()->when;

			if (start < region->position() + region->fade_in()->back()->when)
				fade_in = region->fade_in()->back()->when - (start - region->position());  //end is inside the fade-in, preserve the fade-in endpoint
			break;
		}

		case OverlapExternal:
			fade_in = region->fade_in()->back()->when;
			fade_out = region->fade_out()->back()->when;
			break;
		}

		boost::shared_ptr<AudioRegion> our_region = boost::dynamic_pointer_cast<AudioRegion> (*ours);
		assert (our_region);

		our_region->set_fade_in_length (fade_in);
		our_region->set_fade_out_length (fade_out);
		++ours;
	}

	in_set_state--;

	/* this constructor does NOT notify others (session) */
}

AudioPlaylist::~AudioPlaylist ()
{
	_crossfades.clear ();
}

struct RegionSortByLayer {
    bool operator() (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
	    return a->layer() < b->layer();
    }
};

ARDOUR::framecnt_t
AudioPlaylist::read (Sample *buf, Sample *mixdown_buffer, float *gain_buffer, framepos_t start,
		     framecnt_t cnt, unsigned chan_n)
{
	framecnt_t ret = cnt;

	DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("Playlist %1 read @ %2 for %3, channel %4, regions %5 xfades %6\n",
							   name(), start, cnt, chan_n, regions.size(), _crossfades.size()));

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

	Glib::RecMutex::Lock rm (region_lock);

	framepos_t const end = start + cnt - 1;

	boost::shared_ptr<RegionList> rlist = regions_to_read (start, start+cnt);

	if (rlist->empty()) {
		return cnt;
	}

	map<uint32_t,vector<boost::shared_ptr<Region> > > relevant_regions;
	map<uint32_t,vector<boost::shared_ptr<Crossfade> > > relevant_xfades;
	vector<uint32_t> relevant_layers;

	for (RegionList::iterator i = rlist->begin(); i != rlist->end(); ++i) {
		if ((*i)->coverage (start, end) != OverlapNone) {
			relevant_regions[(*i)->layer()].push_back (*i);
			relevant_layers.push_back ((*i)->layer());
                }
	}

	DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("Checking %1 xfades\n", _crossfades.size()));

	for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("%1 check xfade between %2 and %3 ... [ %4 ... %5 | %6 ... %7]\n",
								   name(), (*i)->out()->name(), (*i)->in()->name(), 
								   (*i)->first_frame(), (*i)->last_frame(),
								   start, end));
		if ((*i)->coverage (start, end) != OverlapNone) {
			relevant_xfades[(*i)->upper_layer()].push_back (*i);
			DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("\t\txfade is relevant (coverage = %2), place on layer %1\n",
									   (*i)->upper_layer(), enum_2_string ((*i)->coverage (start, end))));
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

		DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("read for layer %1\n", *l));

		vector<boost::shared_ptr<Region> > r (relevant_regions[*l]);
		vector<boost::shared_ptr<Crossfade> >& x (relevant_xfades[*l]);


		for (vector<boost::shared_ptr<Region> >::iterator i = r.begin(); i != r.end(); ++i) {
			boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(*i);
                        DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("read from region %1\n", ar->name()));
			assert(ar);
			ar->read_at (buf, mixdown_buffer, gain_buffer, start, cnt, chan_n);
		}

		for (vector<boost::shared_ptr<Crossfade> >::iterator i = x.begin(); i != x.end(); ++i) {
                        DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("read from xfade between %1 & %2\n", (*i)->out()->name(), (*i)->in()->name()));
			(*i)->read_at (buf, mixdown_buffer, gain_buffer, start, cnt, chan_n);
		}
	}

	return ret;
}


void
AudioPlaylist::remove_dependents (boost::shared_ptr<Region> region)
{
	boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion> (region);

	if (in_set_state) {
		return;
	}

	if (r == 0) {
		fatal << _("programming error: non-audio Region passed to remove_overlap in audio playlist")
		      << endmsg;
		return;
	}

	for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ) {

		if ((*i)->involves (r)) {
			i = _crossfades.erase (i);
		} else {
			++i;
		}
	}
}


void
AudioPlaylist::flush_notifications (bool from_undo)
{
	Playlist::flush_notifications (from_undo);

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
	set<boost::shared_ptr<Crossfade> > updated;

	if (ar == 0) {
		return;
	}

	for (Crossfades::iterator x = _crossfades.begin(); x != _crossfades.end();) {

		Crossfades::iterator tmp;

		tmp = x;
		++tmp;

		/* only update them once */

		if ((*x)->involves (ar)) {

			pair<set<boost::shared_ptr<Crossfade> >::iterator, bool> const u = updated.insert (*x);

			if (u.second) {
				/* x was successfully inserted into the set, so it has not already been updated */
				try {
					(*x)->refresh ();
				}

				catch (Crossfade::NoCrossfadeHere& err) {
					// relax, Invalidated during refresh
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

		boost::shared_ptr<Crossfade> fade;

		if ((*x)->_in == orig) {
			if (! (*x)->covers(right->position())) {
				fade = boost::shared_ptr<Crossfade> (new Crossfade (*x, left, (*x)->_out));
			} else {
				// Overlap, the crossfade is copied on the left side of the right region instead
				fade = boost::shared_ptr<Crossfade> (new Crossfade (*x, right, (*x)->_out));
			}
		}

		if ((*x)->_out == orig) {
			if (! (*x)->covers(right->position())) {
				fade = boost::shared_ptr<Crossfade> (new Crossfade (*x, (*x)->_in, right));
			} else {
				// Overlap, the crossfade is copied on the right side of the left region instead
				fade = boost::shared_ptr<Crossfade> (new Crossfade (*x, (*x)->_in, left));
			}
		}

		if (fade) {
			_crossfades.remove (*x);
			add_crossfade (fade);
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
	boost::shared_ptr<Crossfade>   xfade;
	boost::shared_ptr<RegionList> touched_regions;

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


	if (!_session.config.get_auto_xfade()) {
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

                if (other->position() == r->position() && other->length() == r->length()) {
                        /* precise overlay of two regions - no xfade */
                        continue;
                }

		if (other->layer() < region->layer()) {
			top = region;
			bottom = other;
		} else {
			top = other;
			bottom = region;
		}

		if (!top->opaque()) {
			continue;
		}

		OverlapType c = top->coverage (bottom->position(), bottom->last_frame());

		touched_regions.reset ();

		try {
			framecnt_t xfade_length;
			switch (c) {
			case OverlapNone:
				break;

			case OverlapInternal:
				 /* {=============== top  =============}
				  *     [ ----- bottom  ------- ]
				  */
				break;

			case OverlapExternal:

				/*     [ -------- top ------- ]
				 * {=========== bottom =============}
				 */

				/* to avoid discontinuities at the region boundaries of an internal
				   overlap (this region is completely within another), we create
				   two hidden crossfades at each boundary. this is not dependent
				   on the auto-xfade option, because we require it as basic
				   audio engineering.
				*/

				xfade_length = min ((framecnt_t) 720, top->length());

				if (top_region_at (top->first_frame()) == top) {

					xfade = boost::shared_ptr<Crossfade> (new Crossfade (top, bottom, xfade_length, StartOfIn));
					xfade->set_position (top->first_frame());
					add_crossfade (xfade);
				}

				if (top_region_at (top->last_frame() - 1) == top) {

					/*
					   only add a fade out if there is no region on top of the end of 'top' (which
					   would cover it).
					*/

					xfade = boost::shared_ptr<Crossfade> (new Crossfade (bottom, top, xfade_length, EndOfOut));
					xfade->set_position (top->last_frame() - xfade_length);
					add_crossfade (xfade);
				}
				break;
			case OverlapStart:

				/*                   { ==== top ============ }
				 *   [---- bottom -------------------]
				 */

				if (_session.config.get_xfade_model() == FullCrossfade) {
					touched_regions = regions_touched (top->first_frame(), bottom->last_frame());
					if (touched_regions->size() <= 2) {
						xfade = boost::shared_ptr<Crossfade> (new Crossfade (region, other, _session.config.get_xfade_model(), _session.config.get_xfades_active()));
						add_crossfade (xfade);
					}
				} else {

					touched_regions = regions_touched (top->first_frame(),
									   top->first_frame() + min ((framecnt_t) _session.config.get_short_xfade_seconds() * _session.frame_rate(),
												     top->length()));
					if (touched_regions->size() <= 2) {
						xfade = boost::shared_ptr<Crossfade> (new Crossfade (region, other, _session.config.get_xfade_model(), _session.config.get_xfades_active()));
						add_crossfade (xfade);
					}
				}
				break;
			case OverlapEnd:


				/* [---- top ------------------------]
				 *                { ==== bottom ============ }
				 */

				if (_session.config.get_xfade_model() == FullCrossfade) {

					touched_regions = regions_touched (bottom->first_frame(), top->last_frame());
					if (touched_regions->size() <= 2) {
						xfade = boost::shared_ptr<Crossfade> (new Crossfade (region, other,
												     _session.config.get_xfade_model(), _session.config.get_xfades_active()));
						add_crossfade (xfade);
					}

				} else {
					touched_regions = regions_touched (bottom->first_frame(),
									   bottom->first_frame() + min ((framecnt_t)_session.config.get_short_xfade_seconds() * _session.frame_rate(),
													bottom->length()));
					if (touched_regions->size() <= 2) {
						xfade = boost::shared_ptr<Crossfade> (new Crossfade (region, other, _session.config.get_xfade_model(), _session.config.get_xfades_active()));
						add_crossfade (xfade);
					}
				}
				break;
			default:
				xfade = boost::shared_ptr<Crossfade> (new Crossfade (region, other,
										     _session.config.get_xfade_model(), _session.config.get_xfades_active()));
				add_crossfade (xfade);
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
AudioPlaylist::add_crossfade (boost::shared_ptr<Crossfade> xfade)
{
	Crossfades::iterator ci;

	for (ci = _crossfades.begin(); ci != _crossfades.end(); ++ci) {
		if (*(*ci) == *xfade) { // Crossfade::operator==()
			break;
		}
	}

	if (ci != _crossfades.end()) {
		// it will just go away
	} else {
		_crossfades.push_back (xfade);

		xfade->Invalidated.connect_same_thread (*this, boost::bind (&AudioPlaylist::crossfade_invalidated, this, _1));
		xfade->PropertyChanged.connect_same_thread (*this, boost::bind (&AudioPlaylist::crossfade_changed, this, _1));

		notify_crossfade_added (xfade);
	}
}

void AudioPlaylist::notify_crossfade_added (boost::shared_ptr<Crossfade> x)
{
	if (g_atomic_int_get(&block_notifications)) {
		_pending_xfade_adds.insert (_pending_xfade_adds.end(), x);
	} else {
		NewCrossfade (x); /* EMIT SIGNAL */
	}
}

void
AudioPlaylist::crossfade_invalidated (boost::shared_ptr<Region> r)
{
	Crossfades::iterator i;
	boost::shared_ptr<Crossfade> xfade = boost::dynamic_pointer_cast<Crossfade> (r);

	xfade->in()->resume_fade_in ();
	xfade->out()->resume_fade_out ();

	if ((i = find (_crossfades.begin(), _crossfades.end(), xfade)) != _crossfades.end()) {
		_crossfades.erase (i);
	}
}

int
AudioPlaylist::set_state (const XMLNode& node, int version)
{
	XMLNode *child;
	XMLNodeList nlist;
	XMLNodeConstIterator niter;

	in_set_state++;

	if (Playlist::set_state (node, version)) {
		return -1;
	}

	freeze ();

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		child = *niter;

		if (child->name() != "Crossfade") {
			continue;
		}

		try {
			boost::shared_ptr<Crossfade> xfade = boost::shared_ptr<Crossfade> (new Crossfade (*((const Playlist *)this), *child));
			_crossfades.push_back (xfade);
			xfade->Invalidated.connect_same_thread (*this, boost::bind (&AudioPlaylist::crossfade_invalidated, this, _1));
			xfade->PropertyChanged.connect_same_thread (*this, boost::bind (&AudioPlaylist::crossfade_changed, this, _1));
			NewCrossfade(xfade);
		}

		catch (failed_constructor& err) {
			//	cout << string_compose (_("could not create crossfade object in playlist %1"),
			//	  _name)
			//    << endl;
			continue;
		}
	}

	thaw ();
	in_set_state--;

	return 0;
}

void
AudioPlaylist::clear (bool with_signals)
{
	_crossfades.clear ();
	Playlist::clear (with_signals);
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
	boost::shared_ptr<Crossfade> x;

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

        if (!r) {
                return false;
        }

	bool changed = false;
	Crossfades::iterator c, ctmp;
	set<boost::shared_ptr<Crossfade> > unique_xfades;

	{
		RegionLock rlock (this);

		for (RegionList::iterator i = regions.begin(); i != regions.end(); ) {

			RegionList::iterator tmp = i;
			++tmp;

			if ((*i) == region) {
				regions.erase (i);
				changed = true;
			}

			i = tmp;
		}

		for (set<boost::shared_ptr<Region> >::iterator x = all_regions.begin(); x != all_regions.end(); ) {

			set<boost::shared_ptr<Region> >::iterator xtmp = x;
			++xtmp;

			if ((*x) == region) {
				all_regions.erase (x);
				changed = true;
			}

			x = xtmp;
		}

		region->set_playlist (boost::shared_ptr<Playlist>());
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

	if (changed) {
		/* overload this, it normally means "removed", not destroyed */
		notify_region_removed (region);
	}

	return changed;
}

void
AudioPlaylist::crossfade_changed (const PropertyChange&)
{
	if (in_flush || in_set_state) {
		return;
	}

	/* XXX is there a loop here? can an xfade change not happen
	   due to a playlist change? well, sure activation would
	   be an example. maybe we should check the type of change
	   that occured.
	*/

	notify_contents_changed ();
}

bool
AudioPlaylist::region_changed (const PropertyChange& what_changed, boost::shared_ptr<Region> region)
{
	if (in_flush || in_set_state) {
		return false;
	}

	PropertyChange our_interests;

	our_interests.add (Properties::fade_in_active);
	our_interests.add (Properties::fade_out_active);
	our_interests.add (Properties::scale_amplitude);
	our_interests.add (Properties::envelope_active);
	our_interests.add (Properties::envelope);
	our_interests.add (Properties::fade_in);
	our_interests.add (Properties::fade_out);

	bool parent_wants_notify;

	parent_wants_notify = Playlist::region_changed (what_changed, region);

	if (parent_wants_notify || (what_changed.contains (our_interests))) {
		notify_contents_changed ();
	}

	return true;
}

void
AudioPlaylist::crossfades_at (framepos_t frame, Crossfades& clist)
{
	RegionLock rlock (this);

	for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		framepos_t const start = (*i)->position ();
		framepos_t const end = start + (*i)->overlap_length(); // not length(), important difference

		if (frame >= start && frame <= end) {
			clist.push_back (*i);
		}
	}
}

void
AudioPlaylist::foreach_crossfade (boost::function<void (boost::shared_ptr<Crossfade>)> s)
{
	RegionLock rl (this, false);
	for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {
		s (*i);
	}
}

void
AudioPlaylist::update (const CrossfadeListProperty::ChangeRecord& change)
{
	for (CrossfadeListProperty::ChangeContainer::const_iterator i = change.added.begin(); i != change.added.end(); ++i) {
		add_crossfade (*i);
	}

	/* don't remove crossfades here; they will be dealt with by the dependency code */
}

boost::shared_ptr<Crossfade>
AudioPlaylist::find_crossfade (const PBD::ID& id) const
{
	Crossfades::const_iterator i = _crossfades.begin ();
	while (i != _crossfades.end() && (*i)->id() != id) {
		++i;
	}

	if (i == _crossfades.end()) {
		return boost::shared_ptr<Crossfade> ();
	}

	return *i;
}

struct crossfade_triple {
    boost::shared_ptr<Region> old_in;
    boost::shared_ptr<Region> new_in;
    boost::shared_ptr<Region> new_out;
};

void
AudioPlaylist::copy_dependents (const vector<TwoRegions>& old_and_new, Playlist* other) const
{
	AudioPlaylist* other_audio = dynamic_cast<AudioPlaylist*>(other);

	if (!other_audio) {
		return;
	}

	/* our argument is a vector of old and new regions. Each old region
	   might be participant in a crossfade that is already present. Each new
	   region is a copy of the old region, present in the other playlist.

	   our task is to find all the relevant xfades in our playlist (involving
	   the "old" regions) and place copies of them in the other playlist.
	*/

	typedef map<boost::shared_ptr<Crossfade>,crossfade_triple> CrossfadeInfo;
	CrossfadeInfo crossfade_info;

	/* build up a record that links crossfades, old regions and new regions
	 */

	for (vector<TwoRegions>::const_iterator on = old_and_new.begin(); on != old_and_new.end(); ++on) {

		for (Crossfades::const_iterator i = _crossfades.begin(); i != _crossfades.end(); ++i) {

			if ((*i)->in() == on->first) {

				CrossfadeInfo::iterator cf;

				if ((cf = crossfade_info.find (*i)) != crossfade_info.end()) {

					/* already have a record for the old fade-in region,
					   so note the new fade-in region
					*/

					cf->second.new_in = on->second;

				} else {

					/* add a record of this crossfade, keeping an association
					   with the new fade-in region
					*/

					crossfade_triple ct;

					ct.old_in = on->first;
					ct.new_in = on->second;

					crossfade_info[*i] = ct;
				}

			} else if ((*i)->out() == on->first) {

				/* this old region is the fade-out region of this crossfade */

				CrossfadeInfo::iterator cf;

				if ((cf = crossfade_info.find (*i)) != crossfade_info.end()) {

					/* already have a record for this crossfade, so just keep
					   an association for the new fade out region
					*/

					cf->second.new_out = on->second;

				} else {

					/* add a record of this crossfade, keeping an association
					   with the new fade-in region
					*/

					crossfade_triple ct;

					ct.old_in = on->first;
					ct.new_out = on->second;

					crossfade_info[*i] = ct;
				}
			}
		}
	}

	for (CrossfadeInfo::iterator ci = crossfade_info.begin(); ci != crossfade_info.end(); ++ci) {

		/* for each crossfade that involves at least two of the old regions,
		   create a new identical crossfade with the new regions
		*/

		if (!ci->second.new_in || !ci->second.new_out) {
			continue;
		}

		boost::shared_ptr<Crossfade> new_xfade (new Crossfade (ci->first,
								       boost::dynamic_pointer_cast<AudioRegion>(ci->second.new_in),
								       boost::dynamic_pointer_cast<AudioRegion>(ci->second.new_out)));

		/* add it at the right position - which must be at the start
		 * of the fade-in region
		 */

		new_xfade->set_position (ci->second.new_in->position());
		other_audio->add_crossfade (new_xfade);
	}
}

void
AudioPlaylist::pre_combine (vector<boost::shared_ptr<Region> >& copies)
{
	RegionSortByPosition cmp;
	boost::shared_ptr<AudioRegion> ar;

	sort (copies.begin(), copies.end(), cmp);

	ar = boost::dynamic_pointer_cast<AudioRegion> (copies.front());

	/* disable fade in of the first region */

	if (ar) {
		ar->set_fade_in_active (false);
	}

	ar = boost::dynamic_pointer_cast<AudioRegion> (copies.back());

	/* disable fade out of the last region */

	if (ar) {
		ar->set_fade_out_active (false);
	}
}

void
AudioPlaylist::post_combine (vector<boost::shared_ptr<Region> >& originals, boost::shared_ptr<Region> compound_region)
{
	RegionSortByPosition cmp;
	boost::shared_ptr<AudioRegion> ar;
	boost::shared_ptr<AudioRegion> cr;

	if ((cr = boost::dynamic_pointer_cast<AudioRegion> (compound_region)) == 0) {
		return;
	}

	sort (originals.begin(), originals.end(), cmp);

	ar = boost::dynamic_pointer_cast<AudioRegion> (originals.front());

	/* copy the fade in of the first into the compound region */

	if (ar) {
		cr->set_fade_in (ar->fade_in());
	}

	ar = boost::dynamic_pointer_cast<AudioRegion> (originals.back());

	if (ar) {
		/* copy the fade out of the last into the compound region */
		cr->set_fade_out (ar->fade_out());
	}
}

void
AudioPlaylist::pre_uncombine (vector<boost::shared_ptr<Region> >& originals, boost::shared_ptr<Region> compound_region)
{
	RegionSortByPosition cmp;
	boost::shared_ptr<AudioRegion> ar;
	boost::shared_ptr<AudioRegion> cr = boost::dynamic_pointer_cast<AudioRegion>(compound_region);

	if (!cr) {
		return;
	}

	sort (originals.begin(), originals.end(), cmp);

	/* no need to call clear_changes() on the originals because that is
	 * done within Playlist::uncombine ()
	 */

	for (vector<boost::shared_ptr<Region> >::iterator i = originals.begin(); i != originals.end(); ++i) {

		if ((ar = boost::dynamic_pointer_cast<AudioRegion> (*i)) == 0) {
			continue;
		}

		/* scale the uncombined regions by any gain setting for the
		 * compound one.
		 */

		ar->set_scale_amplitude (ar->scale_amplitude() * cr->scale_amplitude());

		if (i == originals.begin()) {

			/* copy the compound region's fade in back into the first
			   original region.
			*/

			if (cr->fade_in()->back()->when <= ar->length()) {
				/* don't do this if the fade is longer than the
				 * region
				 */
				ar->set_fade_in (cr->fade_in());
			}


		} else if (*i == originals.back()) {

			/* copy the compound region's fade out back into the last
			   original region.
			*/

			if (cr->fade_out()->back()->when <= ar->length()) {
				/* don't do this if the fade is longer than the
				 * region
				 */
				ar->set_fade_out (cr->fade_out());
			}

		}

		_session.add_command (new StatefulDiffCommand (*i));
	}
}
