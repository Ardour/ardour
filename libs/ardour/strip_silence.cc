/*
    Copyright (C) 2009-2010 Paul Davis

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

#include "pbd/property_list.h"

#include "ardour/strip_silence.h"
#include "ardour/audioregion.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/dB.h"
#include "ardour/progress.h"

using namespace ARDOUR;

/** Construct a StripSilence filter.
 *  @param s Session.
 *  @param sm Silences to remove.
 *  @param fade_length Length of fade in/out to apply to trimmed regions, in samples.
 */

StripSilence::StripSilence (Session & s, const AudioIntervalMap& sm, framecnt_t fade_length)
	: Filter (s)
        , _smap (sm)
        , _fade_length (fade_length)
{

}

int
StripSilence::run (boost::shared_ptr<Region> r, Progress* progress)
{
	results.clear ();

	/* we only operate on AudioRegions, for now, though this could be adapted to MIDI
	   as well I guess
        */
	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (r);
        InterThreadInfo itt;
        AudioIntervalMap::const_iterator sm;

	if (!region) {
		results.push_back (r);
		return -1;
	}

        if ((sm = _smap.find (r)) == _smap.end()) {
		results.push_back (r);
		return -1;
        }

        const AudioIntervalResult& silence = sm->second;

	if (silence.size () == 1 && silence.front().first == 0 && silence.front().second == region->length() - 1) {
		/* the region is all silence, so just return with nothing */
		return 0;
	}

	if (silence.empty()) {
		/* no silence in this region */
		results.push_back (region);
		return 0;
	}

	/* Turn the silence list into an `audible' list */
	AudioIntervalResult audible;

	/* Add the possible audible section at the start of the region */
	AudioIntervalResult::const_iterator first_silence = silence.begin ();
	if (first_silence->first != region->start()) {
		audible.push_back (std::make_pair (r->start(), first_silence->first));
	}

	/* Add audible sections in the middle of the region */
	for (AudioIntervalResult::const_iterator i = silence.begin (); i != silence.end(); ++i) {
		AudioIntervalResult::const_iterator j = i;
		++j;

		if (j != silence.end ()) {
			audible.push_back (std::make_pair (i->second, j->first));
		}
	}

	/* Add the possible audible section at the end of the region */
	AudioIntervalResult::const_iterator last_silence = silence.end ();
	--last_silence;

	frameoffset_t const end_of_region = r->start() + r->length();

	if (last_silence->second != end_of_region - 1) {
		audible.push_back (std::make_pair (last_silence->second, end_of_region - 1));
	}

	int n = 0;
	int const N = audible.size ();

	for (AudioIntervalResult::const_iterator i = audible.begin(); i != audible.end(); ++i) {

		PBD::PropertyList plist;
		boost::shared_ptr<AudioRegion> copy;

		plist.add (Properties::length, i->second - i->first);
		plist.add (Properties::position, r->position() + (i->first - r->start()));

		copy = boost::dynamic_pointer_cast<AudioRegion> (
			RegionFactory::create (region, (i->first - r->start()), plist)
			);

		copy->set_name (RegionFactory::new_region_name (region->name ()));

		framecnt_t const f = std::min (_fade_length, (i->second - i->first));

		copy->set_fade_in_active (true);
		copy->set_fade_in (FadeLinear, f);
		copy->set_fade_out (FadeLinear, f);
		results.push_back (copy);

		if (progress && (n <= N)) {
			progress->set_progress (float (n) / N);
		}
        }

	return 0;
}
