/*
    Copyright (C) 2017-2020 Paul Davis

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

#include "temporal/beats.h"
#include "temporal/tempo.h"

using namespace Temporal;

Beats
Beats::round_to_subdivision (int subdivision, RoundMode dir) const {

	if (subdivision == 0) {
		/* do not round */
		return *this;
	}

	if (subdivision < 0) {

		/* round to bar */

		TempoMap::SharedPtr map (TempoMap::use());
		const TempoMetric metric (map->metric_at (*this));
		BBT_Time bbt (metric.bbt_at (*this));

		if (dir < 0) {
			bbt = metric.meter().round_down_to_bar (bbt);
		} if (dir > 0) {
			bbt = metric.meter().round_up_to_bar (bbt);
		} else {
			bbt = metric.meter().round_to_bar (bbt);
		}

		return metric.quarters_at (bbt);
	}

	uint32_t ticks = to_ticks();
	const uint32_t ticks_one_subdivisions_worth = ticks_per_beat / subdivision;
	uint32_t mod = ticks % ticks_one_subdivisions_worth;
	uint32_t beats = _beats;

	if (dir > 0) {

		if (mod == 0 && dir == RoundUpMaybe) {
			/* right on the subdivision, which is fine, so do nothing */

		} else if (mod == 0) {
			/* right on the subdivision, so the difference is just the subdivision ticks */
			ticks += ticks_one_subdivisions_worth;

		} else {
			/* not on subdivision, compute distance to next subdivision */

			ticks += ticks_one_subdivisions_worth - mod;
		}

		// NOTE:  this code intentionally limits the rounding so we don't advance to the next beat.
		// For the purposes of "jump-to-next-subdivision", we DO want to advance to the next beat.
		// And since the "prev" direction DOES move beats, I assume this code is unintended.
		// But I'm keeping it around, until we determine there are no terrible consequences.
		// if (ticks >= Temporal::ticks_per_beat) {
		//	ticks -= Temporal::ticks_per_beat;
		// }

	} else if (dir < 0) {

		/* round to previous (or same iff dir == RoundDownMaybe) */

		uint32_t difference = ticks % ticks_one_subdivisions_worth;

		if (difference == 0 && dir == RoundDownAlways) {
			/* right on the subdivision, but force-rounding down,
			   so the difference is just the subdivision ticks */
			difference = ticks_one_subdivisions_worth;
		}

		if (ticks < difference) {
			ticks = ticks_per_beat - ticks;
		} else {
			ticks -= difference;
		}

	} else {
		/* round to nearest */
		double rem;

		/* compute the distance to the previous and next subdivision */

		if ((rem = fmod ((double) ticks, (double) ticks_one_subdivisions_worth)) > ticks_one_subdivisions_worth/2.0) {

			/* closer to the next subdivision, so shift forward */

			ticks = lrint (ticks + (ticks_one_subdivisions_worth - rem));

			//DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved forward to %1\n", ticks));

			if (ticks > ticks_per_beat) {
				++beats;
				ticks -= ticks_per_beat;
				//DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("fold beat to %1\n", beats));
			}

		} else if (rem > 0) {

			/* closer to previous subdivision, so shift backward */

			if (rem > ticks) {
				if (beats == 0) {
					/* can't go backwards past zero, so ... */
					return *this;
				}
				/* step back to previous beat */
				--beats;
				ticks = lrint (ticks_per_beat - rem);
				//DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("step back beat to %1\n", beats));
			} else {
				ticks = lrint (ticks - rem);
				//DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved backward to %1\n", ticks));
			}
		} else {
			/* on the subdivision, do nothing */
		}
	}

	return Beats::ticks (ticks);
}
