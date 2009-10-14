/*
    Copyright (C) 2009 Paul Davis

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

#include "ardour/strip_silence.h"
#include "ardour/audioregion.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/dB.h"

using namespace ARDOUR;

/** Construct a StripSilence filter.
 *  @param s Session.
 *  @param threshold Threshold below which audio is considered silence, in dBFS.
 *  @param minimum_length Minimum length of silence period to recognise, in samples.
 *  @param fade_length Length of fade in/out to apply to trimmed regions, in samples.
 */

StripSilence::StripSilence (Session & s, double threshold, nframes_t minimum_length, nframes_t fade_length)
	: Filter (s), _threshold (threshold), _minimum_length (minimum_length), _fade_length (fade_length)
{

}

int
StripSilence::run (boost::shared_ptr<Region> r)
{
	results.clear ();

	/* we only operate on AudioRegions, for now, though this could be adapted to MIDI
	   as well I guess */
	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (r);
	if (!region) {
		results.push_back (r);
		return -1;
	}

	/* find periods of silence in the region */
	std::list<std::pair<nframes_t, nframes_t> > const silence =
		region->find_silence (dB_to_coefficient (_threshold), _minimum_length);

	if (silence.size () == 1 && silence.front().first == 0 && silence.front().second == region->length() - 1) {
		/* the region is all silence, so just return with nothing */
		return 0;
	}

	if (silence.empty()) {
		/* no silence in this region */
		results.push_back (region);
		return 0;
	}

	std::list<std::pair<nframes_t, nframes_t > >::const_iterator s = silence.begin ();
	nframes_t const pos = region->position ();
	nframes_t const end = region->start () + region->length() - 1;
	nframes_t const start = region->start ();

	region = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region));
	region->set_name (session.new_region_name (region->name ()));
	boost::shared_ptr<AudioRegion> last_region = region;
	results.push_back (region);

	if (s->first == 0) {
		/* the region starts with some silence */

		/* we must set length to an intermediate value here, otherwise the call
		** to set_start will fail */
		region->set_length (region->length() - s->second + _fade_length, 0);
		region->set_start (start + s->second - _fade_length, 0);
		region->set_position (pos + s->second - _fade_length, 0);
		region->set_fade_in_active (true);
		region->set_fade_in (AudioRegion::Linear, _fade_length);
		s++;
	}

	while (s != silence.end()) {

		/* trim the end of this region */
		region->trim_end (pos + s->first + _fade_length, 0);
		region->set_fade_out_active (true);
		region->set_fade_out (AudioRegion::Linear, _fade_length);

		/* make a new region and trim its start */
		region = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region));
		region->set_name (session.new_region_name (region->name ()));
		last_region = region;
		assert (region);
		results.push_back (region);

		/* set length here for the same reasons as above */
		region->set_length (region->length() - s->second + _fade_length, 0);
		region->set_start (start + s->second - _fade_length, 0);
		region->set_position (pos + s->second - _fade_length, 0);
		region->set_fade_in_active (true);
		region->set_fade_in (AudioRegion::Linear, _fade_length);

		s++;
	}

	if (silence.back().second == end) {
		/* the last region we created is zero-sized, so just remove it */
		results.pop_back ();
	} else {
		/* finish off the last region */
		last_region->trim_end (end, 0);
	}

	return 0;
}
