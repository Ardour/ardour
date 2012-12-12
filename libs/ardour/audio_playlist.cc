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
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/region_sorters.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

AudioPlaylist::AudioPlaylist (Session& session, const XMLNode& node, bool hidden)
	: Playlist (session, node, DataType::AUDIO, hidden)
{
#ifndef NDEBUG
	const XMLProperty* prop = node.property("type");
	assert(!prop || DataType(prop->value()) == DataType::AUDIO);
#endif

	in_set_state++;
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor();
	}
	in_set_state--;

	relayer ();

	load_legacy_crossfades (node, Stateful::loading_state_version);
}

AudioPlaylist::AudioPlaylist (Session& session, string name, bool hidden)
	: Playlist (session, name, DataType::AUDIO, hidden)
{
}

AudioPlaylist::AudioPlaylist (boost::shared_ptr<const AudioPlaylist> other, string name, bool hidden)
	: Playlist (other, name, hidden)
{
}

AudioPlaylist::AudioPlaylist (boost::shared_ptr<const AudioPlaylist> other, framepos_t start, framecnt_t cnt, string name, bool hidden)
	: Playlist (other, start, cnt, name, hidden)
{
	RegionReadLock rlock2 (const_cast<AudioPlaylist*> (other.get()));
	in_set_state++;

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
		case Evoral::OverlapNone:
			continue;

		case Evoral::OverlapInternal:
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

		case Evoral::OverlapStart: {
			if (end > region->position() + region->fade_in()->back()->when)
				fade_in = region->fade_in()->back()->when;  //end is after fade-in, preserve the fade-in
			if (end > region->last_frame() - region->fade_out()->back()->when)
				fade_out = region->fade_out()->back()->when - ( region->last_frame() - end );  //end is inside the fadeout, preserve the fades endpoint
			break;
		}

		case Evoral::OverlapEnd: {
			if (start < region->last_frame() - region->fade_out()->back()->when)  //start is before fade-out, preserve the fadeout
				fade_out = region->fade_out()->back()->when;

			if (start < region->position() + region->fade_in()->back()->when)
				fade_in = region->fade_in()->back()->when - (start - region->position());  //end is inside the fade-in, preserve the fade-in endpoint
			break;
		}

		case Evoral::OverlapExternal:
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

/** Sort by descending layer and then by ascending position */
struct ReadSorter {
    bool operator() (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
	    if (a->layer() != b->layer()) {
		    return a->layer() > b->layer();
	    }

	    return a->position() < b->position();
    }
};

/** A segment of region that needs to be read */
struct Segment {
	Segment (boost::shared_ptr<AudioRegion> r, Evoral::Range<framepos_t> a) : region (r), range (a) {}
	
	boost::shared_ptr<AudioRegion> region; ///< the region
	Evoral::Range<framepos_t> range;       ///< range of the region to read, in session frames
};

/** @param start Start position in session frames.
 *  @param cnt Number of frames to read.
 */
ARDOUR::framecnt_t
AudioPlaylist::read (Sample *buf, Sample *mixdown_buffer, float *gain_buffer, framepos_t start,
		     framecnt_t cnt, unsigned chan_n)
{
	DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("Playlist %1 read @ %2 for %3, channel %4, regions %5\n",
							   name(), start, cnt, chan_n, regions.size()));

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

	Playlist::RegionReadLock rl (this);

	/* Find all the regions that are involved in the bit we are reading,
	   and sort them by descending layer and ascending position.
	*/
	boost::shared_ptr<RegionList> all = regions_touched_locked (start, start + cnt - 1);
	all->sort (ReadSorter ());

	/* This will be a list of the bits of our read range that we have
	   handled completely (ie for which no more regions need to be read).
	   It is a list of ranges in session frames.
	*/
	Evoral::RangeList<framepos_t> done;

	/* This will be a list of the bits of regions that we need to read */
	list<Segment> to_do;

	/* Now go through the `all' list filling in `to_do' and `done' */
	for (RegionList::iterator i = all->begin(); i != all->end(); ++i) {
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (*i);

		/* muted regions don't figure into it at all */
		if ( ar->muted() )
			continue;

		/* Work out which bits of this region need to be read;
		   first, trim to the range we are reading...
		*/
		Evoral::Range<framepos_t> region_range = ar->range ();
		region_range.from = max (region_range.from, start);
		region_range.to = min (region_range.to, start + cnt - 1);

		/* ... and then remove the bits that are already done */

		Evoral::RangeList<framepos_t> region_to_do = Evoral::subtract (region_range, done);

		/* Make a note to read those bits, adding their bodies (the parts between end-of-fade-in
		   and start-of-fade-out) to the `done' list.
		*/

		Evoral::RangeList<framepos_t>::List t = region_to_do.get ();

		for (Evoral::RangeList<framepos_t>::List::iterator j = t.begin(); j != t.end(); ++j) {
			Evoral::Range<framepos_t> d = *j;
			to_do.push_back (Segment (ar, d));

			if (ar->opaque ()) {
				/* Cut this range down to just the body and mark it done */
				Evoral::Range<framepos_t> body = ar->body_range ();
				if (body.from < d.to && body.to > d.from) {
					d.from = max (d.from, body.from);
					d.to = min (d.to, body.to);
					done.add (d);
				}
			}
		}
	}

	/* Now go backwards through the to_do list doing the actual reads */
	for (list<Segment>::reverse_iterator i = to_do.rbegin(); i != to_do.rend(); ++i) {
		DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("\tPlaylist %1 read %2 @ %3 for %4, channel %5, buf @ %6 offset %7\n",
								   name(), i->region->name(), i->range.from,
								   i->range.to - i->range.from + 1, (int) chan_n,
								   buf, i->range.from - start));
		i->region->read_at (buf + i->range.from - start, mixdown_buffer, gain_buffer, i->range.from, i->range.to - i->range.from + 1, chan_n);
	}

	return cnt;
}

void
AudioPlaylist::check_crossfades (Evoral::Range<framepos_t> range)
{
	if (in_set_state || in_partition || !_session.config.get_auto_xfade ()) {
		return;
	}

	boost::shared_ptr<RegionList> starts = regions_with_start_within (range);
	boost::shared_ptr<RegionList> ends = regions_with_end_within (range);

	RegionList all = *starts;
	std::copy (ends->begin(), ends->end(), back_inserter (all));

	all.sort (RegionSortByLayer ());

	set<boost::shared_ptr<Region> > done_start;
	set<boost::shared_ptr<Region> > done_end;

	for (RegionList::reverse_iterator i = all.rbegin(); i != all.rend(); ++i) {
		for (RegionList::reverse_iterator j = all.rbegin(); j != all.rend(); ++j) {

			if (i == j) {
				continue;
			}

			if ((*i)->muted() || (*j)->muted()) {
				continue;
			}

			if ((*i)->position() == (*j)->position() && ((*i)->length() == (*j)->length())) {
				/* precise overlay: no xfade */
				continue;
			}

			if ((*i)->position() == (*j)->position() || ((*i)->last_frame() == (*j)->last_frame())) {
				/* starts or ends match: no xfade */
				continue;
			}
			
			boost::shared_ptr<AudioRegion> top;
			boost::shared_ptr<AudioRegion> bottom;
		
			if ((*i)->layer() < (*j)->layer()) {
				top = boost::dynamic_pointer_cast<AudioRegion> (*j);
				bottom = boost::dynamic_pointer_cast<AudioRegion> (*i);
			} else {
				top = boost::dynamic_pointer_cast<AudioRegion> (*i);
				bottom = boost::dynamic_pointer_cast<AudioRegion> (*j);
			}
			
			if (!top->opaque ()) {
				continue;
			}

			Evoral::OverlapType const c = top->coverage (bottom->position(), bottom->last_frame());

			if (c == Evoral::OverlapStart) {
				
				/* top starts within bottom but covers bottom's end */
				
				/*                   { ==== top ============ } 
				 *   [---- bottom -------------------] 
				 */

				if (done_start.find (top) == done_start.end() && done_end.find (bottom) == done_end.end ()) {

					/* Top's fade-in will cause an implicit fade-out of bottom */
					
					if (top->fade_in_is_xfade() && top->fade_in_is_short()) {

						/* its already an xfade. if its
						 * really short, leave it
						 * alone.
						 */

					} else {
						framecnt_t len = 0;
						
						if (_capture_insertion_underway) {
							len = _session.config.get_short_xfade_seconds() * _session.frame_rate();
						} else {
							switch (_session.config.get_xfade_model()) {
							case FullCrossfade:
								len = bottom->last_frame () - top->first_frame () + 1;
								top->set_fade_in_is_short (false);
								break;
							case ShortCrossfade:
								len = _session.config.get_short_xfade_seconds() * _session.frame_rate();
								top->set_fade_in_is_short (true);
								break;
							}
						}

						top->set_fade_in_active (true);
						top->set_fade_in_is_xfade (true);
						
						/* XXX may 2012: -3dB and -6dB curves
						 * are the same right now 
						 */
						
						switch (_session.config.get_xfade_choice ()) {
						case ConstantPowerMinus3dB:
							top->set_fade_in (FadeConstantPower, len);
							break;
						case ConstantPowerMinus6dB:
							top->set_fade_in (FadeConstantPower, len);
							break;
						case RegionFades:
							top->set_fade_in_length (len);
							break;
						}
					}

					done_start.insert (top);
				}

			} else if (c == Evoral::OverlapEnd) {
				
				/* top covers start of bottom but ends within it */
				
				/* [---- top ------------------------] 
				 *                { ==== bottom ============ } 
				 */

				if (done_end.find (top) == done_end.end() && done_start.find (bottom) == done_start.end ()) {
					/* Top's fade-out will cause an implicit fade-in of bottom */
					
					
					if (top->fade_out_is_xfade() && top->fade_out_is_short()) {

						/* its already an xfade. if its
						 * really short, leave it
						 * alone.
						 */

					} else {
						framecnt_t len = 0;
						
						if (_capture_insertion_underway) {
							len = _session.config.get_short_xfade_seconds() * _session.frame_rate();
						} else {
							switch (_session.config.get_xfade_model()) {
							case FullCrossfade:
								len = top->last_frame () - bottom->first_frame () + 1;
								break;
							case ShortCrossfade:
								len = _session.config.get_short_xfade_seconds() * _session.frame_rate();
								break;
							}
						}
						
						top->set_fade_out_active (true);
						top->set_fade_out_is_xfade (true);
						
						switch (_session.config.get_xfade_choice ()) {
						case ConstantPowerMinus3dB:
							top->set_fade_out (FadeConstantPower, len);
							break;
						case ConstantPowerMinus6dB:
							top->set_fade_out (FadeConstantPower, len);
							break;
						case RegionFades:
							top->set_fade_out_length (len);
							break;
						}
					}

					done_end.insert (top);
				}
			}
		}
	}

	for (RegionList::iterator i = starts->begin(); i != starts->end(); ++i) {
		if (done_start.find (*i) == done_start.end()) {
			boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion> (*i);
			if (r->fade_in_is_xfade()) {
				r->set_default_fade_in ();
			}
		}
	}

	for (RegionList::iterator i = ends->begin(); i != ends->end(); ++i) {
		if (done_end.find (*i) == done_end.end()) {
			boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion> (*i);
			if (r->fade_out_is_xfade()) {
				r->set_default_fade_out ();
			}
		}
	}
}

void
AudioPlaylist::dump () const
{
	boost::shared_ptr<Region>r;

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
AudioPlaylist::destroy_region (boost::shared_ptr<Region> region)
{
	boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion> (region);

        if (!r) {
                return false;
        }

	bool changed = false;

	{
		RegionWriteLock rlock (this);

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

	if (changed) {
		/* overload this, it normally means "removed", not destroyed */
		notify_region_removed (region);
	}

	return changed;
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

int
AudioPlaylist::set_state (const XMLNode& node, int version)
{
	return Playlist::set_state (node, version);
}

void
AudioPlaylist::load_legacy_crossfades (const XMLNode& node, int version)
{
	/* Read legacy Crossfade nodes and set up region fades accordingly */

	XMLNodeList children = node.children ();
	for (XMLNodeConstIterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == X_("Crossfade")) {

			XMLProperty* p = (*i)->property (X_("active"));
			assert (p);

			if (!string_is_affirmative (p->value())) {
				continue;
			}
			
			if ((p = (*i)->property (X_("in"))) == 0) {
				continue;
			}

			boost::shared_ptr<Region> in = region_by_id (PBD::ID (p->value ()));

			if (!in) {
				warning << string_compose (_("Legacy crossfade involved an incoming region not present in playlist \"%1\" - crossfade discarded"),
							   name()) 
					<< endmsg;
				continue;
			}

			boost::shared_ptr<AudioRegion> in_a = boost::dynamic_pointer_cast<AudioRegion> (in);
			assert (in_a);

			if ((p = (*i)->property (X_("out"))) == 0) {
				continue;
			}

			boost::shared_ptr<Region> out = region_by_id (PBD::ID (p->value ()));

			if (!out) {
				warning << string_compose (_("Legacy crossfade involved an outgoing region not present in playlist \"%1\" - crossfade discarded"),
							   name()) 
					<< endmsg;
				continue;
			}

			boost::shared_ptr<AudioRegion> out_a = boost::dynamic_pointer_cast<AudioRegion> (out);
			assert (out_a);

			/* now decide whether to add a fade in or fade out
			 * xfade and to which region
			 */

			if (in->layer() <= out->layer()) {

				/* incoming region is below the outgoing one,
				 * so apply a fade out to the outgoing one 
				 */

				const XMLNodeList c = (*i)->children ();
				
				for (XMLNodeConstIterator j = c.begin(); j != c.end(); ++j) {
					if ((*j)->name() == X_("FadeOut")) {
						out_a->fade_out()->set_state (**j, version);
					} else if ((*j)->name() == X_("FadeIn")) {
						out_a->inverse_fade_out()->set_state (**j, version);
					}
				}
				
				if ((p = (*i)->property ("follow-overlap")) != 0) {
					out_a->set_fade_out_is_short (!string_is_affirmative (p->value()));
				} else {
					out_a->set_fade_out_is_short (false);
				}
				
				out_a->set_fade_out_is_xfade (true);
				out_a->set_fade_out_active (true);

			} else {

				/* apply a fade in to the incoming region,
				 * since its above the outgoing one
				 */

				const XMLNodeList c = (*i)->children ();
				
				for (XMLNodeConstIterator j = c.begin(); j != c.end(); ++j) {
					if ((*j)->name() == X_("FadeIn")) {
						in_a->fade_in()->set_state (**j, version);
					} else if ((*j)->name() == X_("FadeOut")) {
						in_a->inverse_fade_in()->set_state (**j, version);
					}
				}
				
				if ((p = (*i)->property ("follow-overlap")) != 0) {
					in_a->set_fade_in_is_short (!string_is_affirmative (p->value()));
				} else {
					in_a->set_fade_in_is_short (false);
				}
				
				in_a->set_fade_in_is_xfade (true);
				in_a->set_fade_in_active (true);
			}
		}
	}
}
