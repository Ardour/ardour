/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#include "pbd/property_list.h"

#include "ardour/strip_silence.h"
#include "ardour/audioregion.h"
#include "ardour/region_factory.h"
#include "ardour/progress.h"

using namespace ARDOUR;

/** Construct a StripSilence filter.
 *  @param s Session.
 *  @param sm Silences to remove.
 *  @param fade_length Length of fade in/out to apply to trimmed regions, in samples.
 */

StripSilence::StripSilence (Session & s, const AudioIntervalMap& sm, samplecnt_t fade_length)
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

	if (silence.size () == 1 && silence.front().first == 0 && silence.front().second == region->length_samples() - 1) {
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
	if (first_silence->first != region->start_sample()) {
		audible.push_back (std::make_pair (r->start_sample(), first_silence->first));
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

	sampleoffset_t const end_of_region = r->start_sample() + r->length_samples();

	if (last_silence->second < end_of_region - 1) {
		audible.push_back (std::make_pair (last_silence->second, end_of_region - 1));
	}

	int n = 0;
	int const N = audible.size ();

	for (AudioIntervalResult::const_iterator i = audible.begin(); i != audible.end(); ++i, ++n) {

		PBD::PropertyList plist;
		boost::shared_ptr<AudioRegion> copy;

		plist.add (Properties::length, i->second - i->first);
		plist.add (Properties::position, r->position_sample() + (i->first - r->start_sample()));

#warning NUTEMPO FIXME need new constructors etc.
//		copy = boost::dynamic_pointer_cast<AudioRegion> (
//			RegionFactory::create (region, MusicSample (i->first - r->start(), 0), plist)
//			);

		copy->set_name (RegionFactory::new_region_name (region->name ()));

		samplecnt_t const f = std::min (_fade_length, (i->second - i->first) / 2);

		if (f > 0) {
			copy->set_fade_in_active (true);
			copy->set_fade_out_active (true);
			copy->set_fade_in (FadeLinear, f);
			copy->set_fade_out (FadeLinear, f);
		} else {
			copy->set_fade_in_active (false);
			copy->set_fade_out_active (false);
		}
		results.push_back (copy);

		if (progress && (n <= N)) {
			progress->set_progress (float (n) / N);
		}
        }

	return 0;
}
