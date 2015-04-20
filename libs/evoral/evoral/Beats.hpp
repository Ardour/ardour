/* This file is part of Evoral.
 * Copyright (C) 2008-2015 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_BEATS_HPP
#define EVORAL_BEATS_HPP

#include <float.h>
#include <math.h>
#include <stdint.h>

#include <iostream>
#include <limits>

#include "evoral/visibility.h"

namespace Evoral {

/** Musical time in beats. */
class /*LIBEVORAL_API*/ Beats {
public:
	LIBEVORAL_API static const double PPQN;

	Beats() : _time(0.0) {}

	/** Create from a real number of beats. */
	explicit Beats(double time) : _time(time) {}

	/** Create from an integer number of beats. */
	static Beats beats(int32_t beats) {
		return Beats((double)beats);
	}

	/** Create from ticks at the standard PPQN. */
	static Beats ticks(uint32_t ticks) {
		return Beats(ticks / PPQN);
	}

	/** Create from ticks at a given rate.
	 *
	 * Note this can also be used to create from frames by setting ppqn to the
	 * number of samples per beat.
	 */
	static Beats ticks_at_rate(uint64_t ticks, uint32_t ppqn) {
		return Beats((double)ticks / (double)ppqn);
	}

	Beats& operator=(const Beats& other) {
		_time = other._time;
		return *this;
	}

	Beats round_up_to_beat() const {
		return Evoral::Beats(ceil(_time));
	}

	Beats round_down_to_beat() const {
		return Evoral::Beats(floor(_time));
	}

	Beats snap_to(const Evoral::Beats& snap) const {
		return Beats(ceil(_time / snap._time) * snap._time);
	}

	inline bool operator==(const Beats& b) const {
		/* Acceptable tolerance is 1 tick. */
		return fabs(_time - b._time) <= (1.0 / PPQN);
	}

	inline bool operator==(double t) const {
		/* Acceptable tolerance is 1 tick. */
		return fabs(_time - t) <= (1.0 / PPQN);
	}

	inline bool operator==(int beats) const {
		/* Acceptable tolerance is 1 tick. */
		return fabs(_time - beats) <= (1.0 / PPQN);
	}

	inline bool operator!=(const Beats& b) const {
		return !operator==(b);
	}

	inline bool operator<(const Beats& b) const {
		/* Acceptable tolerance is 1 tick. */
		if (fabs(_time - b._time) <= (1.0 / PPQN)) {
			return false;  /* Effectively identical. */
		} else {
			return _time < b._time;
		}
	}

	inline bool operator<=(const Beats& b) const {
		return operator==(b) || operator<(b);
	}

	inline bool operator>(const Beats& b) const {
		/* Acceptable tolerance is 1 tick. */
		if (fabs(_time - b._time) <= (1.0 / PPQN)) {
			return false;  /* Effectively identical. */
		} else {
			return _time > b._time;
		}
	}

	inline bool operator>=(const Beats& b) const {
		return operator==(b) || operator>(b);
	}

	inline bool operator<(double b) const {
		/* Acceptable tolerance is 1 tick. */
		if (fabs(_time - b) <= (1.0 / PPQN)) {
			return false;  /* Effectively identical. */
		} else {
			return _time < b;
		}
	}

	inline bool operator<=(double b) const {
		return operator==(b) || operator<(b);
	}

	inline bool operator>(double b) const {
		/* Acceptable tolerance is 1 tick. */
		if (fabs(_time - b) <= (1.0 / PPQN)) {
			return false;  /* Effectively identical. */
		} else {
			return _time > b;
		}
	}

	inline bool operator>=(double b) const {
		return operator==(b) || operator>(b);
	}

	Beats operator+(const Beats& b) const {
		return Beats(_time + b._time);
	}

	Beats operator-(const Beats& b) const {
		return Beats(_time - b._time);
	}

	Beats operator+(double d) const {
		return Beats(_time + d);
	}

	Beats operator-(double d) const {
		return Beats(_time - d);
	}

	Beats operator-() const {
		return Beats(-_time);
	}

	template<typename Number>
	Beats operator*(Number factor) const {
		return Beats(_time * factor);
	}

	Beats& operator+=(const Beats& b) {
		_time += b._time;
		return *this;
	}

	Beats& operator-=(const Beats& b) {
		_time -= b._time;
		return *this;
	}

	double   to_double()              const { return _time; }
	uint64_t to_ticks()               const { return lrint(_time * PPQN); }
	uint64_t to_ticks(uint32_t ppqn)  const { return lrint(_time * ppqn); }

	uint32_t get_beats() const { return floor(_time); }
	uint32_t get_ticks() const { return (uint32_t)lrint(fmod(_time, 1.0) * PPQN); }

	bool operator!() const { return _time == 0; }

	static Beats min()  { return Beats(DBL_MIN); }
	static Beats max()  { return Beats(DBL_MAX); }
	static Beats tick() { return Beats(1.0 / PPQN); }

private:
	double _time;
};

extern LIBEVORAL_API const Beats MaxBeats;
extern LIBEVORAL_API const Beats MinBeats;

/*
  TIL, several horrible hours later, that sometimes the compiler looks in the
  namespace of a type (Evoral::Beats in this case) for an operator, and
  does *NOT* look in the global namespace.

  C++ is proof that hell exists and we are living in it.  In any case, move
  these to the global namespace and PBD::Property's loopy
  virtual-method-in-a-template will bite you.
*/

inline std::ostream&
operator<<(std::ostream& os, const Beats& t)
{
	os << t.to_double();
	return os;
}

inline std::istream&
operator>>(std::istream& is, Beats& t)
{
	double beats;
	is >> beats;
	t = Beats(beats);
	return is;
}

} // namespace Evoral

namespace PBD {
	namespace DEBUG {
		LIBEVORAL_API extern uint64_t Beats;
	}
}

namespace std {
	template<>
	struct numeric_limits<Evoral::Beats> {
		static Evoral::Beats min() { return Evoral::Beats::min(); }
		static Evoral::Beats max() { return Evoral::Beats::max(); }
	};
}

#endif // EVORAL_BEATS_HPP
