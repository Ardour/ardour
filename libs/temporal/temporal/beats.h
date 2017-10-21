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

#ifndef TEMPORAL_BEATS_HPP
#define TEMPORAL_BEATS_HPP

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <iostream>
#include <limits>

#include "temporal/math_utils.h"
#include "temporal/visibility.h"

namespace Temporal {

/** Musical time in beats. */
class /*LIBTEMPORAL_API*/ Beats {
public:
	LIBTEMPORAL_API static const int32_t PPQN = 1920;

	Beats() : _beats(0), _ticks(0) {}

	/** Create from a precise BT time. */
	explicit Beats(int32_t b, int32_t t) {
		normalize(b, t);
	}

	/** Create from a real number of beats. */
	explicit Beats(double time) {
		double       whole;
		const double frac = modf(time, &whole);

		int64_t beats = whole;
		int64_t ticks = round(frac * PPQN);

		normalize(beats, ticks);
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

		int64_t beats = whole;
		int64_t ticks = round(frac * PPQN);
		normalize(beats, ticks);
		return *this;
	}

	Beats& operator=(const Beats& other) {
		_beats = other._beats;
		_ticks = other._ticks;
		return *this;
	}

	Beats round_to_beat() const {
		int32_t extra_ticks;
		div_rtni(_ticks, PPQN, &extra_ticks);
		if (extra_ticks >= PPQN / 2) {
			return round_up_to_beat();
		}
		else {
			return round_down_to_beat();
		}
	}

	Beats round_up_to_beat() const {
		int32_t remainder;
		div_rtni(_ticks, PPQN, &remainder);
		return make_normalized(_beats,
		                       ((int64_t) _ticks) + PPQN - remainder);
	}

	Beats round_down_to_beat() const {
		int32_t remainder;
		div_rtni(_ticks, PPQN, &remainder);
		return make_normalized(_beats,
		                       ((int64_t) _ticks) - remainder);
	}

	Beats snap_to(const Temporal::Beats& snap) const {
		const double snap_time = abs(snap.to_double());
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
		return *this == b || *this < b;
	}

	inline bool operator>(const Beats& b) const {
		return _beats > b._beats || (_beats == b._beats && _ticks > b._ticks);
	}

	inline bool operator>=(const Beats& b) const {
		return *this == b || *this > b;
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
		return make_normalized(_beats + (int64_t) b._beats,
		                       _ticks + (int64_t) b._ticks);
	}

	Beats operator-(const Beats& b) const {
		return make_normalized(_beats - (int64_t) b._beats,
		                       _ticks - (int64_t) b._ticks);
	}

	Beats operator+(double d) const {
		return Beats(to_double() + d);
	}

	Beats operator-(double d) const {
		return Beats(to_double() - d);
	}

	Beats& operator+=(double d) {
		*this = Beats(to_double() + d);
		return *this;
	}

	Beats& operator-=(double d) {
		*this = Beats(to_double() - d);
		return *this;
	}

	Beats operator+(int b) const {
		return make_normalized(_beats + (int64_t) b, _ticks);
	}

	Beats operator-(int b) const {
		return make_normalized(_beats - (int64_t) b, _ticks);
	}

	Beats& operator+=(int b) {
		normalize(_beats + (int64_t) b, _ticks);
		return *this;
	}

	Beats& operator-=(int b) {
		normalize(_beats - (int64_t) b, _ticks);
		return *this;
	}

	Beats operator-() const {
		return make_normalized(- (int64_t) _beats, - (int64_t) _ticks);
	}

	Beats operator/(float factor) const {
		return Beats(to_double() / factor);
	}

	Beats& operator+=(const Beats& b) {
		normalize(_beats + (int64_t) b._beats, _ticks + (int64_t) b._ticks);
		return *this;
	}

	Beats& operator-=(const Beats& b) {
		normalize(_beats - (int64_t) b._beats, _ticks - (int64_t) b._ticks);
		return *this;
	}

	template<typename Number>
	Beats operator*(Number factor) const {
		return make_normalized(0,
		                       round((((int64_t) _beats) * PPQN + _ticks) * factor));
	}

	template<typename Number>
	Beats operator/(Number factor) const {
		return make_normalized(0,
		                       round((((int64_t) _beats) * PPQN + _ticks) / factor));
	}

	double  to_double()              const { return (double)_beats + (_ticks / (double)PPQN); }
	int64_t to_ticks()               const { return (int64_t)_beats * PPQN + _ticks; }
	int64_t to_ticks(uint32_t ppqn)  const { return (int64_t)_beats * ppqn + ((int64_t)_ticks * ppqn / PPQN); }

	/**
	 * Gets the number of whole beats rounded down, if this number fits in an
	 * int32_t. If the number of whole beats is outside the range of int32_t,
	 * the closest int32_t value is returned, and the remaining beats are
	 * represented by having get_ticks return a value outside the range
	 * [0, PPQN).
	 */
	int32_t get_beats() const { return _beats; }

	/**
	 * Gets the number of ticks representing the fractional part of the beats.
	 * Always returns a value in the range [0, PPQN), EXCEPT if the true
	 * number of beats is outside the range of int32_t.
	 */
	int32_t get_ticks() const { return _ticks; }

	bool operator!() const { return _beats == 0 && _ticks == 0; }

	static Beats tick() { return Beats(0, 1); }

private:
	// Number of whole beats, rounded toward negative infinity. If the number
	// of whole beats does not fit in an int32_t, some of the beats will be
	// represented by a _ticks value outside the range [0, PPQN) (see below).
	int32_t _beats;
	
	// Number of ticks forming the fractional part of the beats.
	// This should always be in the range [0, PPQN) after normalization,
	// EXCEPT if the true number of beats is outside the range of an int32_t.
	// In that case, _ticks may be negative and/or larger than the number of
	// ticks in a beat. This issue arises mainly in computing
	// std::numeric_limits<Temporal::Beats>::max() and
	// std::numeric_limits<Temporal::Beats>::lowest().
	int32_t _ticks;

	/**
	 * Normalizes the specified beats and ticks, and sets _beats and _ticks to
	 * the result. Attempts to produce a result where _ticks is in the range
	 * [0, PPQN) and _beats is inside the range of an int32_t. Arguments
	 * accepted are 64 bits wide in order to gracefully handle calculations
	 * where the number of beats is too large to fit in an int32_t, in which
	 * case the extra beats are stored implicitly in _ticks.
	 */
	void normalize(int64_t beats, int64_t ticks) {
		int64_t new_beats = beats;
		int64_t new_ticks = ticks;

		// Normalize so new_ticks is in the range [0, PPQN).
		if (new_ticks < 0) {
			int64_t beats_to_shift = div_rtni(new_ticks, (int64_t) PPQN,
			                                  (int64_t*) 0);
			shift_beats(beats_to_shift, new_beats, new_ticks);
		}
		if (new_ticks >= PPQN) {
			int64_t beats_to_shift = new_ticks / PPQN;
			shift_beats(beats_to_shift, new_beats, new_ticks);
		}

		// Adjust so new_beats is inside the range of an int32_t. This might
		// move new_ticks outside the range [0, PPQN), but that is what we want
		// if the true number of beats can't fit in an int32_t.
		if (new_beats < std::numeric_limits<int32_t>::lowest()) {
			shift_beats(std::numeric_limits<int32_t>::lowest() - new_beats,
			            new_beats, new_ticks);
		}
		if (new_beats > std::numeric_limits<int32_t>::max()) {
			shift_beats(std::numeric_limits<int32_t>::max() - new_beats,
			            new_beats, new_ticks);
		}

		// Convert to int32_t and store.
		_beats = new_beats;
		_ticks = new_ticks;
	}

	/**
	 * Similar to normalize(), except it stores the result in a new Beats object
	 * instead of *this.
	 */
	static Beats make_normalized(int64_t beats, int64_t ticks) {
		Beats result;
		result.normalize(beats, ticks);
		return result;
	}

	/**
	 * Adjusts the beats argument by the specified amount, and adjusts the ticks
	 * argument by the opposite amount to keep the overall value the same.
	 */
	static void shift_beats(int64_t num_to_shift, int64_t& beats,
	                        int64_t& ticks)
	{
		beats += num_to_shift;
		ticks -= num_to_shift * PPQN;
	}
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
	int32_t beats;
	int32_t ticks;
	char separator;

	is >> beats >> separator >> ticks;
	
	// Separator should be '.'
	if (separator != '.') {
		is.setstate(std::ios_base::failbit);
	}

	t = Beats(beats, ticks);

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
			return Temporal::Beats(std::numeric_limits<int32_t>::min(),
			                     std::numeric_limits<int32_t>::min());
		}

		/* We don't define min() since this has different behaviour for integral and floating point types,
		   but Beats is used as both.  Better to avoid providing a min at all
		   than a confusing one. */

		static Temporal::Beats max() {
			return Temporal::Beats(std::numeric_limits<int32_t>::max(),
			                     std::numeric_limits<int32_t>::max());
		}
	};
}

#endif // TEMPORAL_BEATS_HPP
