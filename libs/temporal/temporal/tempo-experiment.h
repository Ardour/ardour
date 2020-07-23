/*
    Copyright (C) 2020 Paul Davis

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

#ifndef __libtemporal_tempo_h__
#define __libtemporal_tempo_h__

#include <cassert>
#include <cmath>
#include <exception>
#include <limits>
#include <ostream>
#include <string>

#include "pbd/enumwriter.h"
#include "pbd/integer_division.h"

#include "temporal/beats.h"
#include "temporal/types.h"
#include "temporal/superclock.h"
#include "temporal/visibility.h"

/************************* !!!! ATTENTION !!!! *************************

	   NO FLOATING POINT ARITHMETIC IS ALLOWED IN THIS HEADER OR
		       ANY OF THE OBJECTS DEFINED HERE.

    Exceptions:

      1) constructors/methods that accept double from the user
      2) clearly labelled methods that warn a developer to avoid
         their use or use them only for display purposes.

************************************************************************/


namespace Temporal {

class TempoValue {
  private:
	/* beats per minute * big_numerator => rational number expressing (possibly fractional) bpm as superbeats-per-minute
	 *
	 * It is not required that big_numerator equal superclock_ticks_per_second but since the values in both cases have similar
	 * desired properties (many, many factors), it doesn't hurt to use the same number.
	 */
	static const superclock_t big_numerator = superclock_ticks_per_second;

  public:

	TempoValue (double bpm) : val (bpm) {

		/* since we allow the user to provide bpm as a floating point value,
		   we allow use of floating point math to determine the two critical
		   integer values (superbeats-per-second and superclocks-per-beat
		*/
		_sbps = (superclock_t) llround (bpm * (big_numerator / 60));
		_scpb = (superclock_t) llround ((60./ bpm) * superclock_ticks_per_second);
	}

	double given_bpm_for_display_only () const { return val; }
	double actual_bpm_for_display_only ()  const { return (_sbps * 60) / (double) big_numerator; }
	uint64_t ticks_per_second() const { return int_div_round ((_sbps * Temporal::ticks_per_beat), big_numerator); }
	superclock_t superclocks_per_beat() const { return _scpb; }

	Temporal::Beats superclocks_as_beats (superclock_t sc) const {
		/* convert sc into superbeats, given that sc represents some number of seconds */
		const superclock_t whole_seconds = sc / superclock_ticks_per_second;
		const superclock_t remainder = sc - (whole_seconds * superclock_ticks_per_second);
		const superclock_t superbeats = (_sbps * whole_seconds) + int_div_round ((_sbps * remainder), superclock_ticks_per_second);

		/* convert superbeats to beats:ticks */

		uint32_t b = superbeats / big_numerator;
		const uint64_t remain = superbeats - (b * big_numerator);
		uint32_t t = int_div_round ((Temporal::ticks_per_beat * remain), big_numerator);

		return Beats (b, t);
	}

	superclock_t beats_as_superclocks (Temporal::Beats const & b) const {
		/* no symmetrical with superclocks_as_beats() because Beats already breaks apart the beats:ticks,
		   with the ticks value denominated by Temporal::ticks_per_beat
		*/
		return (_scpb * b.get_beats()) + int_div_round ((_scpb * b.get_ticks()), superclock_t (Temporal::ticks_per_beat));
	}

	Temporal::Beats seconds_as_beats (uint64_t num, uint64_t denom) const {
		return superclocks_as_beats ((num * superclock_ticks_per_second) / denom);
	}

	double beats_as_float_seconds_avoid_me (Temporal::Beats const & b) const {
		return beats_as_superclocks (b) / (double) superclock_ticks_per_second;
	}

  private:
	double val;         /* as given to constructor */
	uint64_t     _sbps; /* superbeats-per-second */
	superclock_t _scpb; /* superclocks-per-beat */
};

inline std::ostream&
operator<<(std::ostream& os, const TempoValue& v)
{
	os << v.actual_bpm_for_display_only ();
	return os;
}

inline std::istream&
operator>>(std::istream& istr, TempoValue& v)
{
#if 0
	uint16_t w;
	uint64_t f;
	char d; /* delimiter, whatever it is */
	istr >> w >> d >> f;
	v = TempoValue (w, f);
#endif
	return istr;
}

} /* namespace Temporal */

#endif /* __libtemporal_tempo_h__ */
