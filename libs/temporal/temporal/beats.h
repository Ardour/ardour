/*
 * Copyright (C) 2017-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef TEMPORAL_BEATS_HPP
#define TEMPORAL_BEATS_HPP

#include <cassert>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <iostream>
#include <limits>
#include <sstream>

#include "pbd/failed_constructor.h"
#include "pbd/integer_division.h"
#include "pbd/string_convert.h"


#include "temporal/visibility.h"
#include "temporal/types.h"

namespace ARDOUR {
class Variant; /* Can stay since LV2 has no way to exchange beats as anything except double */
/* these all need fixing to not use ::to_double() */
class Track;
class MidiStretch;
class MidiModel;
class AutomationList;
class MidiSource;
class MidiRegion;
class Quantize;
}

namespace Evoral {
template<typename T> class Sequence;
}

/* XXX hack friends for ::do_double() access ... remove */

class QuantizeDialog;
class NoteDrag;
class NoteCreateDrag;

namespace Temporal {

/** Musical time in beats. */
class /*LIBTEMPORAL_API*/ Beats {
public:
	LIBTEMPORAL_API static const int32_t PPQN = Temporal::ticks_per_beat;

	Beats() : _ticks(0) {}
	Beats(const Beats& other) : _ticks(other._ticks) {}

	/** Create from a precise beats:ticks pair. */
	explicit Beats(int64_t b, int64_t t) : _ticks ((b*PPQN) + t) {}

	/** Create from a real number of beats. */
	static Beats from_double (double beats) {
		double       whole;
		const double frac = modf (beats, &whole);
		return Beats (whole, (int32_t) rint (frac * PPQN));
	}

	/** Create from an integer number of beats. */
	static Beats beats(int64_t beats) {
		return Beats (beats, 0);
	}

	/** Create from ticks at the standard PPQN. */
	static Beats ticks(int64_t ticks) {
		return Beats (0, ticks);
	}

	/** Create from ticks at a given rate.
	 *
	 * Note this can also be used to create from frames by setting ppqn to the
	 * number of samples per beat.  Note the resulting Beats will, like all
	 * others, have the default PPQN, so this is a potentially lossy
	 * conversion.
	 */
	static Beats ticks_at_rate(int64_t ticks, uint32_t ppqn) {
		return Beats(ticks / ppqn, (ticks % ppqn) * PPQN / ppqn);
	}

	int64_t to_ticks ()               const { return _ticks; }
	int64_t to_ticks (uint32_t ppqn)  const { return (_ticks * ppqn) / PPQN; }

	int64_t get_beats () const { return _ticks / PPQN; }
	int32_t get_ticks () const { return _ticks % PPQN; }

	Beats& operator=(double time) {
		*this = from_double (time);
		return *this;
	}

	Beats& operator=(const Beats& other) {
		_ticks = other._ticks;
		return *this;
	}

  public:
	Beats round_up_to_multiple  (Beats const & multiple) const {
		return ticks (((to_ticks() + (multiple.to_ticks() - 1)) / multiple.to_ticks()) * multiple.to_ticks());
	}
	Beats round_to_multiple  (Beats const & multiple) const {
		return ticks (((to_ticks() + (int_div_round (multiple.to_ticks(), (int64_t) 2))) / multiple.to_ticks()) * multiple.to_ticks());
	}
	Beats round_down_to_multiple (Beats const & multiple) const {
		return ticks ((to_ticks() / multiple.to_ticks()) * multiple.to_ticks());
	}

	Beats round_to_beat() const {
		return (get_ticks() >= (PPQN/2)) ? Beats (get_beats() + 1, 0) : Beats (get_beats(), 0);
	}

	Beats round_up_to_beat() const {
		return (get_ticks() == 0) ? *this : Beats(get_beats() + 1, 0);
	}

	Beats round_down_to_beat() const {
		return Beats(get_beats(), 0);
	}


	Beats prev_beat() const {
		/* always moves backwards even if currently on beat */
		return Beats (get_beats()-1, 0);
	}

	Beats next_beat() const {
		/* always moves forwards even if currently on beat */
		return Beats (get_beats()+1, 0);
	}

	LIBTEMPORAL_API Beats round_to_subdivision (int subdivision, RoundMode dir) const;

	Beats abs () const {
		return ticks (::abs (_ticks));
	}

	Beats diff (Beats const & other) const {
		if (other > *this) {
			return other - *this;
		}
		return *this - other;
	}

	inline bool operator==(const Beats& b) const {
		return _ticks == b._ticks;
	}

	inline bool operator==(int beats) const {
		return get_beats() == beats;
	}

	inline bool operator!=(const Beats& b) const {
		return !operator==(b);
	}

	inline bool operator<(const Beats& b) const {
		return _ticks < b._ticks;
	}

	inline bool operator<=(const Beats& b) const {
		return _ticks <= b._ticks;
	}

	inline bool operator>(const Beats& b) const {
		return _ticks > b._ticks;
	}

	inline bool operator>=(const Beats& b) const {
		return _ticks >= b._ticks;
	}

	Beats operator+(const Beats& b) const {
		return ticks (_ticks + b._ticks);
	}

	Beats operator-(const Beats& b) const {
		return ticks (_ticks - b._ticks);
	}

	Beats operator-() const {
		return ticks (-_ticks);
	}

	Beats operator*(int32_t factor) const {return ticks (_ticks * factor); }
	Beats operator/(int32_t factor) const { return ticks (_ticks / factor);}
	Beats operator*(ratio_t const & factor) const {return ticks (PBD::muldiv (_ticks, factor.numerator(), factor.denominator())); }
	Beats operator/(ratio_t const & factor) const {return ticks (PBD::muldiv (_ticks, factor.denominator(), factor.numerator())); }

	Beats operator% (Beats const & b) { return Beats::ticks (_ticks % b.to_ticks());}

	Beats operator%= (Beats const & b) {
		const Beats B (Beats::ticks (to_ticks() % b.to_ticks()));
		_ticks = B._ticks;
		return *this;
	}

	Beats operator/ (Beats const & other) const {
		return Beats::ticks (int_div_round (to_ticks(), other.to_ticks()));
	}

	Beats operator* (Beats const & other) const {
		return Beats::ticks (to_ticks () * other.to_ticks());
	}

	Beats& operator+=(const Beats& b) {
		_ticks += b._ticks;
		return *this;
	}

	Beats& operator-=(const Beats& b) {
		_ticks -= b._ticks;
		return *this;
	}

	bool operator!() const { return _ticks == 0; }
	explicit operator bool () const { return _ticks != 0; }

	static Beats one_tick() { return Beats(0, 1); }

  protected:
	int64_t _ticks;

};

/* Only contexts that really, absolutely need a floating point representation
 * of a Beats value should ever use this.
 */

class DoubleableBeats : public Beats
{
     public:
	DoubleableBeats (Beats const & b) : Beats (b) {}
	double to_double() const { return (double)get_beats() + (get_ticks() / (double)PPQN); }
};


/*
  TIL, several horrible hours later, that sometimes the compiler looks in the
  namespace of a type (Temporal::Beats in this case) for an operator, and
  does *NOT* look in the global namespace.

  C++ is proof that hell exists and we are living in it.  In any case, move
  these to the global namespace and PBD::Property's loopy
  virtual-method-in-a-template will bite you.
*/

LIBTEMPORAL_API std::ostream& operator<<(std::ostream& ostream, const Temporal::Beats& t);
LIBTEMPORAL_API std::istream& operator>>(std::istream& istream, Temporal::Beats& b);

} // namespace Temporal

namespace std {
	template<>
	struct numeric_limits<Temporal::Beats> {
		static Temporal::Beats lowest() {
			return Temporal::Beats(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min());
		}

		/* We don't define min() since this has different behaviour for
		   integral and floating point types, but Beats is used as both
		   an integral and "fractional" value, so the semantics of
		   min() would be unclear.

		   Better to avoid providing a min at all than a confusing one.
		*/

		/* We must make the number of beats be 1 less than INT32_MAX,
		 * because otherwise adding the PPQN-1 ticks would cause
		 * overflow (the value would be INT32_MAX+((PPQN-1)/PPQN) which
		 * exceeds INT32_MAX.
		 */

		static Temporal::Beats max() {
			return Temporal::Beats(std::numeric_limits<int32_t>::max() - 1, Temporal::Beats::PPQN - 1);
		}
	};

}

namespace PBD {

template<>
inline bool to_string (Temporal::Beats val, std::string & str)
{
	std::ostringstream ostr;
	ostr << val;
	str = ostr.str();
	return true;
}

template<>
inline bool string_to (std::string const & str, Temporal::Beats & val)
{
	std::istringstream istr (str);
	istr >> val;
	return (bool) istr;
}

} /* end namsepace PBD */


#endif // TEMPORAL_BEATS_HPP
