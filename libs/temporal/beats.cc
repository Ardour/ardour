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

#include "pbd/integer_division.h"

#include "temporal/beats.h"
#include "temporal/debug.h"
#include "temporal/tempo.h"

using namespace PBD;
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

	DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("%4 => round-nearest, ticks %1 1 div of ticks %2 mod %3\n", ticks, ticks_one_subdivisions_worth, mod, *this));

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

		/* round to previous (or same iff di == RoundDownMaybe) */

		if (mod == 0 && dir == RoundDownAlways) {
			/* right on the subdivision, but force-rounding down,
			   so the modulo (difference) is just the subdivision
			   ticks
			*/
			mod = ticks_one_subdivisions_worth;
		}

		if (ticks < mod) {
			ticks = ticks_per_beat - ticks;
		} else {
			ticks -= mod;
		}

	} else {

		/* round to nearest, which happens to be precisely what
		 * PBD::int_div_round() does. Set beats to zero, since the
		 * Beats normalization will set the values correctly in the
		 * value we actually return
		 */

		ticks = int_div_round (ticks, ticks_one_subdivisions_worth) * ticks_one_subdivisions_worth;
		beats = 0;

	}

	DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("return %1 from %2 : %3\n", Beats (beats, ticks), beats, ticks));
	return Beats (beats, ticks);
}
