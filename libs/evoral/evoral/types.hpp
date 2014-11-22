/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
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

#ifndef EVORAL_TYPES_HPP
#define EVORAL_TYPES_HPP

#include <float.h>
#include <math.h>
#include <stdint.h>

#include <iostream>
#include <limits>
#include <list>

#include "evoral/visibility.h"

#include "pbd/debug.h"

namespace Evoral {

/** ID of an event (note or other). This must be operable on by glib
    atomic ops
*/
typedef int32_t event_id_t;

/** Musical time: beats relative to some defined origin */
class LIBEVORAL_API MusicalTime {
public:
	MusicalTime() : _time(0.0) {}

	/** Create from a real number of beats. */
	explicit MusicalTime(double time) : _time(time) {}

	/** Create from an integer number of beats. */
	static MusicalTime beats(int32_t beats) {
		return MusicalTime((double)beats);
	}

	/** Create from ticks at the standard PPQN. */
	static MusicalTime ticks(uint32_t ticks) {
		return MusicalTime(ticks / _ppqn);
	}

	/** Create from ticks at a given rate.
	 *
	 * Note this can also be used to create from frames by setting ppqn to the
	 * number of samples per beat.
	 */
	static MusicalTime ticks_at_rate(uint64_t ticks, uint32_t ppqn) {
		return MusicalTime((double)ticks / (double)ppqn);
	}

	MusicalTime& operator=(const MusicalTime& other) {
		_time = other._time;
		return *this;
	}

	MusicalTime round_up_to_beat() const {
		return Evoral::MusicalTime(ceil(_time));
	}

	MusicalTime round_down_to_beat() const {
		return Evoral::MusicalTime(floor(_time));
	}

	MusicalTime snap_to(const Evoral::MusicalTime& snap) const {
		return MusicalTime(ceil(_time / snap._time) * snap._time);
	}

	inline bool operator==(const MusicalTime& b) const {
		/* Acceptable tolerance is 1 tick. */
		return fabs(_time - b._time) <= (1.0/_ppqn);
	}

	inline bool operator==(double t) const {
		/* Acceptable tolerance is 1 tick. */
		return fabs(_time - t) <= (1.0/_ppqn);
	}

	inline bool operator==(int beats) const {
		/* Acceptable tolerance is 1 tick. */
		return fabs(_time - beats) <= (1.0/_ppqn);
	}

	inline bool operator!=(const MusicalTime& b) const {
		return !operator==(b);
	}

	inline bool operator<(const MusicalTime& b) const {
		/* Acceptable tolerance is 1 tick. */
		if (fabs(_time - b._time) <= (1.0/_ppqn)) {
			return false;  /* Effectively identical. */
		} else {
			return _time < b._time;
		}
	}

	inline bool operator<=(const MusicalTime& b) const {
		return operator==(b) || operator<(b);
	}

	inline bool operator>(const MusicalTime& b) const {
		/* Acceptable tolerance is 1 tick. */
		if (fabs(_time - b._time) <= (1.0/_ppqn)) {
			return false;  /* Effectively identical. */
		} else {
			return _time > b._time;
		}
	}

	inline bool operator>=(const MusicalTime& b) const {
		/* Acceptable tolerance is 1 tick. */
		if (fabs(_time - b._time) <= (1.0/_ppqn)) {
			return true;  /* Effectively identical. */
		} else {
			return _time >= b._time;
		}
	}

	MusicalTime operator+(const MusicalTime& b) const {
		return MusicalTime(_time + b._time);
	}

	MusicalTime operator-(const MusicalTime& b) const {
		return MusicalTime(_time - b._time);
	}

	MusicalTime operator-() const {
		return MusicalTime(-_time);
	}

	template<typename Number>
	MusicalTime operator*(Number factor) const {
		return MusicalTime(_time * factor);
	}

	MusicalTime& operator+=(const MusicalTime& b) {
		_time += b._time;
		return *this;
	}

	MusicalTime& operator-=(const MusicalTime& b) {
		_time -= b._time;
		return *this;
	}

	double   to_double()              const { return _time; }
	uint64_t to_ticks()               const { return lrint(_time * _ppqn); }
	uint64_t to_ticks(uint32_t ppqn)  const { return lrint(_time * ppqn); }

	operator bool() const { return _time != 0; }

	static MusicalTime min()  { return MusicalTime(DBL_MIN); }
	static MusicalTime max()  { return MusicalTime(DBL_MAX); }
	static MusicalTime tick() { return MusicalTime(1.0 / _ppqn); }

private:
	static const double _ppqn = 1920.0;  /* TODO: Make configurable. */

	double _time;
};

const MusicalTime MaxMusicalTime = Evoral::MusicalTime::max();
const MusicalTime MinMusicalTime = Evoral::MusicalTime::min();

/** Type of an event (opaque, mapped by application) */
typedef uint32_t EventType;

/*
  TIL, several horrible hours later, that sometimes the compiler looks in the
  namespace of a type (Evoral::MusicalTime in this case) for an operator, and
  does *NOT* look in the global namespace.

  C++ is proof that hell exists and we are living in it.  In any case, move
  these to the global namespace and PBD::Property's loopy
  virtual-method-in-a-template will bite you.
*/

inline std::ostream&
operator<<(std::ostream& os, const MusicalTime& t)
{
	os << t.to_double();
	return os;
}

inline std::istream&
operator>>(std::istream& is, MusicalTime& t)
{
	double beats;
	is >> beats;
	t = MusicalTime(beats);
	return is;
}

} // namespace Evoral

namespace PBD {
	namespace DEBUG {
		LIBEVORAL_API extern uint64_t Sequence;
		LIBEVORAL_API extern uint64_t Note;
		LIBEVORAL_API extern uint64_t ControlList;
		LIBEVORAL_API extern uint64_t MusicalTime;
	}
}

namespace std {
	template<>
	struct numeric_limits<Evoral::MusicalTime> {
		static Evoral::MusicalTime min() { return Evoral::MusicalTime::min(); }
		static Evoral::MusicalTime max() { return Evoral::MusicalTime::max(); }
	};
}

#endif // EVORAL_TYPES_HPP
