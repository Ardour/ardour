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
 *  @param threshold Threshold below which audio is considered silence, in dBFS.
 *  @param minimum_length Minimum length of silence period to recognise, in samples.
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

        AudioIntervalResult::const_iterator s = silence.begin ();
        PBD::PropertyList plist;
        framepos_t start;
        framepos_t end;
        bool in_silence;
        boost::shared_ptr<AudioRegion> copy;

        start = r->start();

        if (s->first == start) {
                /* segment starting at zero is silent */
                end = s->second;
                in_silence = true;
        } else {
                /* segment starting at zero is audible, and begins at the start of the region in the source */
                end = s->first;
                in_silence = false;
        }

	int n = 0;
	int const N = silence.size ();

        while (start < r->start() + r->length()) {

                framecnt_t interval_duration;

                interval_duration = end - start;

                if (!in_silence && interval_duration > 0) {

                        plist.clear ();
                        plist.add (Properties::length, interval_duration);
                        plist.add (Properties::position, r->position() + (start - r->start()));

                        copy = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create 
                                                                         (region, (start - r->start()), plist));

                        copy->set_name (RegionFactory::new_region_name (region->name ()));

                        copy->set_fade_in_active (true);
                        copy->set_fade_in (FadeLinear, _fade_length);
                        results.push_back (copy);
                }

                start = end;
                in_silence = !in_silence;
                ++s;

                if (s == silence.end()) {
                        end = r->start() + r->length();
                } else {
                        end = s->first;
                }

		++n;

		if (progress && (n <= N)) {
			progress->set_progress (float (n) / N);
		}

        }

	return 0;
}
