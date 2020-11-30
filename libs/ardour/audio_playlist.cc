/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2006 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2006 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2012 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <algorithm>

#include <cstdlib>

#include "ardour/types.h"
#include "ardour/debug.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/region_sorters.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

AudioPlaylist::AudioPlaylist (Session& session, const XMLNode& node, bool hidden)
	: Playlist (session, node, DataType::AUDIO, hidden)
{
#ifndef NDEBUG
	XMLProperty const * prop = node.property("type");
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

AudioPlaylist::AudioPlaylist (boost::shared_ptr<const AudioPlaylist> other, timepos_t const & start, timepos_t const & cnt, string name, bool hidden)
	: Playlist (other, start, cnt, name, hidden)
{
	RegionReadLock rlock2 (const_cast<AudioPlaylist*> (other.get()));
	in_set_state++;

	const timepos_t tend = start + cnt;
	samplepos_t end = tend.samples();

	/* Audio regions that have been created by the Playlist constructor
	   will currently have the same fade in/out as the regions that they
	   were created from.  This is wrong, so reset the fades here.
	*/

	RegionList::iterator ours = regions.begin ();

	for (RegionList::const_iterator i = other->regions.begin(); i != other->regions.end(); ++i) {
		boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (*i);
		assert (region);

		samplecnt_t fade_in = 64;
		samplecnt_t fade_out = 64;

		switch (region->coverage (start, tend)) {
		case Temporal::OverlapNone:
			continue;

		case Temporal::OverlapInternal:
		{
			samplecnt_t const offset = start.samples() - region->position_sample ();
			samplecnt_t const trim = region->last_sample() - end;
			if (region->fade_in()->back()->when > offset) {
				fade_in = region->fade_in()->back()->when.earlier (timepos_t (offset)).samples();
			}
			if (region->fade_out()->back()->when > trim) {
				fade_out = region->fade_out()->back()->when.earlier (timepos_t (trim)).samples();
			}
			break;
		}

		case Temporal::OverlapStart: {
			if (timepos_t (end) > region->position() + region->fade_in()->back()->when) {
				fade_in = region->fade_in()->back()->when.samples();  //end is after fade-in, preserve the fade-in
			}
			if (timepos_t (end) >= region->end().earlier (region->fade_out()->back()->when)) {
				fade_out = region->fade_out()->back()->when.earlier (timepos_t (region->last_sample() - end)).samples();  //end is inside the fadeout, preserve the fades endpoint
			}
			break;
		}

		case Temporal::OverlapEnd: {
			if (start < region->end().earlier (region->fade_out()->back()->when)) {  //start is before fade-out, preserve the fadeout
				fade_out = region->fade_out()->back()->when.samples();
			}
			if (start < region->position() + region->fade_in()->back()->when) {
				fade_in = region->fade_in()->back()->when.earlier (start.distance (region->position())).samples();  //end is inside the fade-in, preserve the fade-in endpoint
			}
			break;
		}

		case Temporal::OverlapExternal:
			fade_in = region->fade_in()->back()->when.samples();
			fade_out = region->fade_out()->back()->when.samples();
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
	Segment (boost::shared_ptr<AudioRegion> r, Temporal::Range a) : region (r), range (a) {}

	boost::shared_ptr<AudioRegion> region; ///< the region
	Temporal::Range range;       ///< range of the region to read, in session samples
};

/** @param start Start position in session samples.
 *  @param cnt Number of samples to read.
 */
ARDOUR::timecnt_t
AudioPlaylist::read (Sample *buf, Sample *mixdown_buffer, float *gain_buffer, timepos_t const & start, timecnt_t const & cnt, uint32_t chan_n)
{
	DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("Playlist %1 read @ %2 for %3, channel %4, regions %5 mixdown @ %6 gain @ %7\n",
							   name(), start, cnt, chan_n, regions.size(), mixdown_buffer, gain_buffer));

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

	memset (buf, 0, sizeof (Sample) * cnt.samples());

	/* this function is never called from a realtime thread, so
	   its OK to block (for short intervals).
	*/

	Playlist::RegionReadLock rl (this);

	/* Find all the regions that are involved in the bit we are reading,
	   and sort them by descending layer and ascending position.
	*/
	boost::shared_ptr<RegionList> all = regions_touched_locked (start, start + cnt);
	all->sort (ReadSorter ());

	/* This will be a list of the bits of our read range that we have
	   handled completely (ie for which no more regions need to be read).
	   It is a list of ranges in session samples.
	*/
	Temporal::RangeList done;

	/* This will be a list of the bits of regions that we need to read */
	list<Segment> to_do;

	/* Now go through the `all' list filling in `to_do' and `done' */
	for (RegionList::iterator i = all->begin(); i != all->end(); ++i) {
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (*i);

		/* muted regions don't figure into it at all */
		if (ar->muted()) {
			continue;
		}

		/* check for the case of solo_selection */
		const bool force_transparent = (_session.solo_selection_active() && SoloSelectedActive() && !SoloSelectedListIncludes( (const Region*) &(**i)));
		if (force_transparent) {
			continue;
		}

		/* Work out which bits of this region need to be read;
		   first, trim to the range we are reading...
		*/
		Temporal::Range rrange = ar->range_samples ();
		Temporal::Range region_range (max (rrange.start(), start),
		                              min (rrange.end(), start + cnt));

		/* ... and then remove the bits that are already done */

		Temporal::RangeList region_to_do = region_range.subtract (done);

		/* Make a note to read those bits, adding their bodies (the parts between end-of-fade-in
		   and start-of-fade-out) to the `done' list.
		*/

		Temporal::RangeList::List t = region_to_do.get ();

		for (Temporal::RangeList::List::iterator j = t.begin(); j != t.end(); ++j) {
			Temporal::Range d = *j;
			to_do.push_back (Segment (ar, d));

			if (ar->opaque ()) {
				/* Cut this range down to just the body and mark it done */
				Temporal::Range body = ar->body_range ();

				if (body.start() < d.end() && body.end() > d.start()) {
					d.set_start (max (d.start(), body.start()));
					d.set_end (min (d.end(), body.end()));
					done.add (d);
				}
			}
		}
	}

	/* Now go backwards through the to_do list doing the actual reads */

	for (list<Segment>::reverse_iterator i = to_do.rbegin(); i != to_do.rend(); ++i) {
		DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("\tPlaylist %1 read %2 @ %3 for %4, channel %5, buf @ %6 offset %7\n",
		                                                   name(), i->region->name(), i->range.start(),
		                                                   i->range.length(), (int) chan_n,
		                                                   buf, i->range.start().earlier (start)));

		i->region->read_at (buf + start.distance (i->range.start()).samples(), mixdown_buffer, gain_buffer, i->range.start().samples(), i->range.start().distance (i->range.end()).samples(), chan_n);
	}

	return cnt;
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

	PropertyChange bounds;
	bounds.add (Properties::start);
	bounds.add (Properties::position);
	bounds.add (Properties::length);

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
	/* if bounds changed, we have already done notify_contents_changed ()*/
	if ((parent_wants_notify || what_changed.contains (our_interests)) && !what_changed.contains (bounds)) {
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

			XMLProperty const * p = (*i)->property (X_("active"));
			assert (p);

			if (!string_to<bool> (p->value())) {
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

				in_a->set_fade_in_active (true);
			}
		}
	}
}
