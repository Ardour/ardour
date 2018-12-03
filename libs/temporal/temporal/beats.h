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

#include "temporal/visibility.h"

namespace Temporal {

/** Musical time in beats. */
class /*LIBTEMPORAL_API*/ Beats {
public:
	LIBTEMPORAL_API static const int32_t PPQN = 1920;

	Beats() : _beats(0), _ticks(0) {}
	Beats(const Beats& other) : _beats(other._beats), _ticks(other._ticks) {}

	/** Normalize so ticks is within PPQN. */
	void normalize() {
		// First, fix negative ticks with positive beats
		while (_beats > 0 && _ticks < 0) {
			--_beats;
			_ticks += PPQN;
		}

		// Now fix positive ticks with negative beats
		while (_beats < 0 && _ticks > 0) {
			++_beats;
			_ticks -= PPQN;
		}

		assert ((_beats < 0 && _ticks <= 0) || (_beats > 0 && _ticks >= 0) || _beats == 0);

		// Work with positive beats and ticks to normalize
		const int32_t sign  = _beats < 0 ? -1 : _ticks < 0 ? -1 : 1;
		int32_t       beats = abs(_beats);
		int32_t       ticks = abs(_ticks);

		// Fix ticks greater than 1 beat
		while (ticks >= PPQN) {
			++beats;
			ticks -= PPQN;
		}

		// Set fields with appropriate sign
		_beats = sign * beats;
		_ticks = sign * ticks;
	}

	/** Create from a precise BT time. */
	explicit Beats(int32_t b, int32_t t) : _beats(b), _ticks(t) {
		normalize();
	}

	/** Create from a real number of beats. */
	explicit Beats(double time) {
		double       whole;
		const double frac = modf(time, &whole);

		_beats = whole;
		_ticks = frac * PPQN;
	}

	/** Create from an integer number of beats. */
	static Beats beats(int32_t beats) {
		return Beats(beats, 0);
	}

	/** Create from ticks at the standard PPQN. */
	static Beats ticks(int32_t ticks) {
		return Beats(0, ticks);
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

	Beats& operator=(double time) {
		double       whole;
		const double frac = modf(time, &whole);

		_beats = whole;
		_ticks = frac * PPQN;
		return *this;
	}

	Beats& operator=(const Beats& other) {
		_beats = other._beats;
		_ticks = other._ticks;
		return *this;
	}

	Beats round_to_beat() const {
		return (_ticks >= (PPQN/2)) ? Beats (_beats + 1, 0) : Beats (_beats, 0);
	}

	Beats round_up_to_beat() const {
		return (_ticks == 0) ? *this : Beats(_beats + 1, 0);
	}

	Beats round_down_to_beat() const {
		return Beats(_beats, 0);
	}

	Beats snap_to(const Temporal::Beats& snap) const {
		const double snap_time = snap.to_double();
		return Beats(ceil(to_double() / snap_time) * snap_time);
	}

	inline bool operator==(const Beats& b) const {
		return _beats == b._beats && _ticks == b._ticks;
	}

	inline bool operator==(double t) const {
		/* Acceptable tolerance is 1 tick. */
		return fabs(to_double() - t) <= (1.0 / PPQN);
	}

	inline bool operator==(int beats) const {
		return _beats == beats;
	}

	inline bool operator!=(const Beats& b) const {
		return !operator==(b);
	}

	inline bool operator<(const Beats& b) const {
		return _beats < b._beats || (_beats == b._beats && _ticks < b._ticks);
	}

	inline bool operator<=(const Beats& b) const {
		return _beats < b._beats || (_beats == b._beats && _ticks <= b._ticks);
	}

	inline bool operator>(const Beats& b) const {
		return _beats > b._beats || (_beats == b._beats && _ticks > b._ticks);
	}

	inline bool operator>=(const Beats& b) const {
		return _beats > b._beats || (_beats == b._beats && _ticks >= b._ticks);
	}

	inline bool operator<(double b) const {
		/* Acceptable tolerance is 1 tick. */
		const double time = to_double();
		if (fabs(time - b) <= (1.0 / PPQN)) {
			return false;  /* Effectively identical. */
		} else {
			return time < b;
		}
	}

	inline bool operator<=(double b) const {
		return operator==(b) || operator<(b);
	}

	inline bool operator>(double b) const {
		/* Acceptable tolerance is 1 tick. */
		const double time = to_double();
		if (fabs(time - b) <= (1.0 / PPQN)) {
			return false;  /* Effectively identical. */
		} else {
			return time > b;
		}
	}

	inline bool operator>=(double b) const {
		return operator==(b) || operator>(b);
	}

	Beats operator+(const Beats& b) const {
		return Beats(_beats + b._beats, _ticks + b._ticks);
	}

	Beats operator-(const Beats& b) const {
		return Beats(_beats - b._beats, _ticks - b._ticks);
	}

	Beats operator+(double d) const {
		return Beats(to_double() + d);
	}

	Beats operator-(double d) const {
		return Beats(to_double() - d);
	}

	Beats operator+(int b) const {
		return Beats (_beats + b, _ticks);
	}

	Beats operator-(int b) const {
		return Beats (_beats - b, _ticks);
	}

	Beats& operator+=(int b) {
		_beats += b;
		return *this;
	}

	Beats& operator-=(int b) {
		_beats -= b;
		return *this;
	}

	Beats operator-() const {
		/* must avoid normalization here, which will convert a negative
		   value into a valid beat position before zero, which is not
		   we want here.
		*/
		Beats b (_beats, _ticks);
		b._beats = -b._beats;
		b._ticks = -b._ticks;
		return b;
	}

	template<typename Number>
	Beats operator*(Number factor) const {
		return ticks ((_beats * PPQN + _ticks) * factor);
	}

	template<typename Number>
	Beats operator/(Number factor) const {
		return ticks ((_beats * PPQN + _ticks) / factor);
	}

	Beats operator% (Beats const & b) {
		return Beats::ticks (to_ticks() % b.to_ticks());
	}

	Beats operator%= (Beats const & b) {
		const Beats B (Beats::ticks (to_ticks() % b.to_ticks()));
		_beats = B._beats;
		_ticks = B._ticks;
		return *this;
	}

	Beats& operator+=(const Beats& b) {
		_beats += b._beats;
		_ticks += b._ticks;
		normalize();
		return *this;
	}

	Beats& operator-=(const Beats& b) {
		_beats -= b._beats;
		_ticks -= b._ticks;
		normalize();
		return *this;
	}

	double  to_double()              const { return (double)_beats + (_ticks / (double)PPQN); }
	int64_t to_ticks()               const { return (int64_t)_beats * PPQN + _ticks; }
	int64_t to_ticks(uint32_t ppqn)  const { return (int64_t)_beats * ppqn + (_ticks * ppqn / PPQN); }

	int32_t get_beats() const { return _beats; }
	int32_t get_ticks() const { return _ticks; }

	bool operator!() const { return _beats == 0 && _ticks == 0; }

	static Beats tick() { return Beats(0, 1); }

private:
	int32_t _beats;
	int32_t _ticks;
};

/*
  TIL, several horrible hours later, that sometimes the compiler looks in the
  namespace of a type (Temporal::Beats in this case) for an operator, and
  does *NOT* look in the global namespace.

  C++ is proof that hell exists and we are living in it.  In any case, move
  these to the global namespace and PBD::Property's loopy
  virtual-method-in-a-template will bite you.
*/

inline std::ostream&
operator<<(std::ostream& os, const Beats& t)
{
	os << t.get_beats() << '.' << t.get_ticks();
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
		LIBTEMPORAL_API extern uint64_t Beats;
	}
}

namespace std {
	template<>
	struct numeric_limits<Temporal::Beats> {
		static Temporal::Beats lowest() {
			return Temporal::Beats(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min());
		}

		/* We don't define min() since this has different behaviour for integral and floating point types,
		   but Beats is used as both.  Better to avoid providing a min at all
		   than a confusing one. */

		static Temporal::Beats max() {
			return Temporal::Beats(std::numeric_limits<int32_t>::max(), Temporal::Beats::PPQN-1);
		}
	};
}

#endif // TEMPORAL_BEATS_HPP
