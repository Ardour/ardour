/*
 * Copyright (C) 2020 Paul Davis
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

#ifndef __libtemporal_timeline_h__
#define __libtemporal_timeline_h__

#include <ostream>
#include <exception>
#include <string>
#include <cassert>
#include <limits>

#include "pbd/enumwriter.h"
#include "pbd/int62.h"

#include "temporal/types.h"
#include "temporal/beats.h"
#include "temporal/bbt_time.h"
#include "temporal/superclock.h"
#include "temporal/visibility.h"

namespace Temporal {

class timecnt_t;

/* 62 bit positional time value. Theoretically signed, but the intent is for to
 * always be positive. If the flag bit is set (i.e. ::flagged() is true), the
 * numerical value counts musical ticks; otherwise it counts superclocks.
 */

class LIBTEMPORAL_API timepos_t : public int62_t  {
  public:
	timepos_t () : int62_t (false, 0) {}
	explicit timepos_t (timecnt_t const &); /* will throw() if val is negative */
	explicit timepos_t (Temporal::Beats const & b) : int62_t (false, b.to_ticks()) {}

	/* superclock_t and samplepos_t are the same underlying primitive type,
	 * which means we cannot use polymorphism to differentiate them.
	 */
	static timepos_t from_superclock (superclock_t s)  { return timepos_t (false, s); }
	static timepos_t from_samples (superclock_t s)  { return timepos_t (false, samples_to_superclock (s, _thread_sample_rate)); }

	bool is_beats() const { return flagged(); }
	bool is_superclock() const { return !flagged(); }

	Temporal::TimeDomain time_domain () const { if (flagged()) return Temporal::BeatTime; return Temporal::AudioTime; }

	superclock_t superclocks() const { if (is_superclock()) return v; return _superclocks (); }
	int64_t samples() const { return superclock_to_samples (superclocks(), _thread_sample_rate); }
	int64_t ticks() const { if (is_beats()) return val(); return _ticks (); }
	Beats beats() const { if (is_beats()) return Beats::ticks (val()); return _beats (); }

	timepos_t & operator= (timecnt_t const & t); /* will throw() if val is negative */
	timepos_t & operator= (superclock_t s) { v = s; return *this; }
	timepos_t & operator= (Temporal::Beats const & b) { operator= (build (true, b.to_ticks())); return *this; }

	bool operator== (timepos_t const & other) const { return v == other.v; }
	bool operator!= (timepos_t const & other) const { return v != other.v; }

	bool operator<  (timecnt_t const & other) const;
	bool operator>  (timecnt_t const & other) const;
	bool operator<= (timecnt_t const & other) const;
	bool operator>= (timecnt_t const & other) const;

	bool operator<  (timepos_t const & other) const { if (is_beats() == other.is_beats()) return val() < other.val(); return expensive_lt (other); }
	bool operator>  (timepos_t const & other) const { if (is_beats() == other.is_beats()) return val() > other.val(); return expensive_gt (other); }
	bool operator<= (timepos_t const & other) const { if (is_beats() == other.is_beats()) return val() <= other.val(); return expensive_lte (other); }
	bool operator>= (timepos_t const & other) const { if (is_beats() == other.is_beats()) return val() >= other.val(); return expensive_gte (other); }

	timepos_t operator+(timecnt_t const & d) const;
	timepos_t operator+(timepos_t const & d) const { if (is_beats() == d.is_beats()) return timepos_t (is_beats(), val() + d.val()); return expensive_add (d); }

	timepos_t operator+(Temporal::Beats const &b ) const { if (is_beats()) return timepos_t (true, ticks() + b.to_ticks()); return expensive_add (b); }

	/* donn't provide operator+(samplepos_t) or operator+(superclock_t)
	 * because the compiler can't disambiguate them and neither can we.
	 * to add such types, use ::from_samples() or ::from_superclock() to
	 * create a timepo_t and then add that.
	 */

	/* operator-() poses severe and thorny problems for a class that represents position on a timeline.
	 *
	 * If the value of the class is a simple scalar, then subtraction can be used for both:
	 *
	 *    1) movement backwards along the timeline
	 *    2) computing the distance between two positions
	 *
	 * But timepos_t is not a simple scalar, and neither is timecnt_t, and these two operations are quite different.
	 *
	 *    1) movement backwards along the timeline should result in another timepos_t
	 *    2) the distance between two positions is a timecnt_t
	 *
	 * so already we have a hint that we would need at least:
	 *
	 *    timepos_t operator- (timecnt_t const &); ... compute new position
	 *    timecnt_t operator- (timepos_t const &); ... compute distance
	 *
	 * But what happens we try to use more explicit types. What does this expression mean:
	 *
	 *    timepos_t pos;
	 *    pos - Beats (3);
	 *
	 * is this computing a new position 3 beats earlier than pos? or is it computing the distance between
	 * pos and the 3rd beat?
	 *
	 * For this reason, we do not provide any operator-() methods, but instead require the use of
	 * explicit methods with clear semantics.
	*/

	/* computes the distance between this timepos_t and @param p
	 * such that: this + distance = p
	 *
	 * This means that if @param p is later than this, distance is positive;
	 * if @param p is earlier than this, distance is negative.

	 * Note that the return value is a timecnt_t whose position member
	 * is equal to the value of this. That means if the distance uses
	 * musical time value, the distance may not have constant value
	 * at other positions on the timeline.
	*/

	timecnt_t distance (timecnt_t const & p) const;
	timecnt_t distance (Temporal::Beats const & b) const;
	timecnt_t distance (timepos_t const & p) const;

	/* computes a new position value that is @param d earlier than this */
	timepos_t earlier (timepos_t const & d) const; /* treat d as distance measured from timeline origin */
	timepos_t earlier (timecnt_t const & d) const;
	timepos_t earlier (Beats const & d) const;
	timepos_t earlier (BBT_Offset const & d) const;

	/* like ::earlier() but changes this. loosely equivalent to operator-= */
	timepos_t & shift_earlier (timecnt_t const & d);
	timepos_t & shift_earlier (Temporal::Beats const &);
	timepos_t & shift_earlier (Temporal::BBT_Offset const &);

	/* given the absence of operator- and thus also operator--, return a
	 * timepos_t that is the previous (earlier) possible position given
	 * this one
	 */
	timepos_t decrement () const { return timepos_t (false, val() > 0 ? val() - 1 : val()); /* cannot go negative */ }

	/* purely for reasons of symmetry with ::decrement(), return a
	 * timepos_t that is the next (later) possible position given this one
	 */
	timepos_t increment () const { return timepos_t (flagged(), val() + 1); }

	timepos_t & operator+=(timecnt_t const & d);
	timepos_t & operator+=(timepos_t const & d);
	timepos_t & operator+=(Temporal::Beats const &);
	timepos_t & operator+=(Temporal::BBT_Offset const &);

	timepos_t   operator% (timecnt_t const &) const;
	timepos_t & operator%=(timecnt_t const &);

	bool operator<  (superclock_t s) { return v < s; }
	bool operator<  (Temporal::Beats const & b) { return beats() < b; }
	bool operator<= (superclock_t s) { return v <= s; }
	bool operator<= (Temporal::Beats const & b) { return beats() <= b; }
	bool operator>  (superclock_t s) { return v > s; }
	bool operator>  (Temporal::Beats const & b) { return beats() > b; }
	bool operator>= (superclock_t s) { return v >= s; }
	bool operator>= (Temporal::Beats const & b) { return beats() >= b; }
	bool operator== (superclock_t s) { return v == s; }
	bool operator== (Temporal::Beats const & b) { return beats() == b; }
	bool operator!= (superclock_t s) { return v != s; }
	bool operator!= (Temporal::Beats const & b) { return beats() != b; }

	void set_superclock (superclock_t s);
	void set_beat (Temporal::Beats const &);
	void set_bbt (Temporal::BBT_Time const &);

	bool string_to (std::string const & str);
	std::string to_string () const;

	static timepos_t const & max() { return _max_timepos; }

  private:
	int64_t v;
	/* special private constructor for use when constructing timepos_t as a
	   return value using arithmetic ops
	*/
	explicit timepos_t (bool b, int64_t v) : int62_t (b, v) {}

	static timepos_t _max_timepos;

	/* these can only be called after verifying that the time domain does
	 * not match the relevant one i.e. call _beats() to get a Beats value
	 * when this is using the audio time doamin
	 */

	superclock_t _superclocks() const;
	int64_t _ticks() const;
	Beats _beats() const;

	bool expensive_lt (timepos_t const &) const;
	bool expensive_lte (timepos_t const &) const;
	bool expensive_gt (timepos_t const &) const;
	bool expensive_gte(timepos_t const &) const;

	bool expensive_lt (timecnt_t const &) const;
	bool expensive_lte (timecnt_t const &) const;
	bool expensive_gt (timecnt_t const &) const;
	bool expensive_gte(timecnt_t const &) const;

	/* used to compute distance when time domains do not match */

	timecnt_t expensive_distance (timepos_t const & p) const;
	timecnt_t expensive_distance (Beats const &) const;

	timepos_t expensive_add (Temporal::Beats const &) const;
	timepos_t expensive_add (timepos_t const & s) const;

	using int62_t::operator int64_t;
	using int62_t::operator-;
	using int62_t::operator-=;
};


/**
 * a timecnt_t measures a duration in a specified time domain and starting at a
 * specific position.
 *
 * It can be freely converted between time domains, as well as used as the
 * subject of most arithmetic operations.
 *
 * An important distinction between timepos_t and timecnt_t can be thought of
 * this way: a timepos_t ALWAYS refers to a position relative to the origin of
 * the timeline (technically, the origin in the tempo map used to translate
 * between audio and musical domains). By contrast, a timecnt_t refers to a
 * certain distance beyond some arbitrary (specified) origin. So, a timepos_t
 * of "3 beats" always means "3 beats measured from the timeline origin". A
 * timecnt_t of "3 beats" always come with a position, and so is really "3
 * beats after <position>".
 *
 * The ambiguity surrounding operator-() that affects timepos_t does not exist
 * for timecnt_t: all uses of operator-() are intended to compute the result of
 * subtracting one timecnt_t from another which will always result in another
 * timecnt_t of lesser value than the first operand.
 */

class LIBTEMPORAL_API timecnt_t {
   public:
	/* default to zero superclocks @ zero */
	timecnt_t () : _distance (int62_t (false, 0)), _position (timepos_t::from_superclock (0)) {}
	timecnt_t (timecnt_t const &other) : _distance (other.distance()), _position (other.position()) {}

	/* construct from timeline types */
	explicit timecnt_t (timepos_t const & d, timepos_t const & p) : _distance (d), _position (p) { assert (p.is_beats() == d.is_beats()); }
	explicit timecnt_t (timecnt_t const &, timepos_t const & pos);

	/* construct from int62_t (which will be flagged or not) and timepos_t */
	explicit timecnt_t (int62_t d, timepos_t p) : _distance (d), _position (p) { assert (p.is_beats() == d.flagged()); }

	/* construct from beats */
	explicit timecnt_t (Temporal::Beats const & b, timepos_t const & pos) :  _distance (true, b.to_ticks()), _position (pos) { assert ( _distance.flagged() == _position.is_beats()); }

	/* superclock_t and samplepos_t are the same underlying primitive type,
	 * which means we cannot use polymorphism to differentiate them.
	 */
	static timecnt_t from_superclock (superclock_t s, timepos_t const & pos) { return timecnt_t (int62_t (false, s), pos); }
	static timecnt_t from_samples (samplepos_t s, timepos_t const & pos) { return timecnt_t (int62_t (false, samples_to_superclock (s, _thread_sample_rate)), pos); }

	/* Construct from just a distance value - position is assumed to be zero */
	explicit timecnt_t (Temporal::Beats const & b) :  _distance (true, b.to_ticks()), _position (Beats()) {}
	static timecnt_t from_superclock (superclock_t s) { return timecnt_t (int62_t (false, s), timepos_t::from_superclock (0)); }
	static timecnt_t from_samples (samplepos_t s) { return timecnt_t (int62_t (false, samples_to_superclock (s, _thread_sample_rate)), timepos_t::from_superclock (0)); }

	int62_t const & distance() const { return _distance; }
	timepos_t const & position() const { return _position; }
	void set_position (timepos_t const &pos);

	bool positive() const { return _distance.val() > 0; }
	bool negative() const {return _distance.val() < 0; }
	bool zero() const { return _distance.val() == 0; }

	static timecnt_t const & max() { return _max_timecnt; }

	timecnt_t abs() const;

	Temporal::TimeDomain time_domain () const { return _position.time_domain (); }

	superclock_t    superclocks() const { if (_position.is_superclock()) return _distance.val(); return compute_superclocks(); }
	int64_t         samples() const { return superclock_to_samples (superclocks(), _thread_sample_rate); }
	Temporal::Beats beats  () const { if (_position.is_beats()) return Beats::ticks (_distance.val()); return compute_beats(); }
	int64_t         ticks  () const { if (_position.is_beats()) return _distance.val(); return compute_ticks(); }

	timecnt_t & operator= (superclock_t s) { _distance = int62_t (false, s); return *this; }
	timecnt_t & operator= (Temporal::Beats const & b) { _distance = int62_t (true, b.to_ticks()); return *this; }

	/* return a timecnt_t that is the next/previous (earlier/later) possible position given
	 * this one
	 */
	timecnt_t operator++ () { _distance += 1; return *this; }
	timecnt_t operator-- () { _distance -= 1; return *this; }

	timecnt_t operator*(ratio_t const &) const;
	timecnt_t operator/(ratio_t const &) const;

	timecnt_t operator-() const;
	timecnt_t operator- (timecnt_t const & t) const;
	timecnt_t operator+ (timecnt_t const & t) const;

	timecnt_t & operator-= (timecnt_t const & t);
	timecnt_t & operator+= (timecnt_t const & t);

	//timecnt_t operator- (timepos_t const & t) const;
	//timecnt_t operator+ (timepos_t const & t) const;
	//timecnt_t & operator-= (timepos_t);
	//timecnt_t & operator+= (timepos_t);

	bool operator> (timecnt_t const & other) const { return _distance > other.distance (); }
	bool operator>= (timecnt_t const & other) const { return _distance >= other.distance(); }
	bool operator< (timecnt_t const & other) const { return _distance < other.distance(); }
	bool operator<= (timecnt_t const & other) const { return _distance <= other.distance(); }
	timecnt_t & operator=(timecnt_t const & other) {
		if (this != &other) {
			if (_distance.flagged() == other.distance().flagged()) {
				_distance = other.distance();
			} else {
				/* unclear what to do here but we cannot allow
				   inconsistent timecnt_t to be created
				*/
			}
		}
		return *this;
	}

	bool operator!= (timecnt_t const & other) const { return _distance != other.distance(); }
	bool operator== (timecnt_t const & other) const { return _distance == other.distance(); }

	/* test for numerical equivalence with a timepos_T. This tests ONLY the
	   duration in the given domain, NOT position.
	*/
	bool operator== (timepos_t const & other) const { return _distance == other; }

	bool operator< (Temporal::samplepos_t s) { return samples() < s; }
	bool operator< (Temporal::Beats const & b) { return beats() < b; }
	bool operator<= (Temporal::samplepos_t s) { return samples() <= s; }
	bool operator<= (Temporal::Beats const & b) { return beats() <= b; }
	bool operator> (Temporal::samplepos_t s) { return samples() > s; }
	bool operator> (Temporal::Beats const & b) { return beats() > b; }
	bool operator>= (Temporal::samplepos_t s) { return samples() >= s; }
	bool operator>= (Temporal::Beats const & b) { return beats() >= b; }
	bool operator== (Temporal::samplepos_t s) { return samples() == s; }
	bool operator== (Temporal::Beats const & b) { return beats() == b; }
	bool operator!= (Temporal::samplepos_t s) { return samples() != s; }
	bool operator!= (Temporal::Beats const & b) { return beats() != b; }

	timecnt_t   operator% (timecnt_t const &) const;
	timecnt_t & operator%=(timecnt_t const &);

	bool string_to (std::string const & str);
	std::string to_string () const;

  private:
	int62_t   _distance; /* aka "duration" */
	timepos_t _position; /* aka "origin */

	static timecnt_t _max_timecnt;

	superclock_t compute_superclocks () const;
	Beats compute_beats () const;
	int64_t compute_ticks () const;
};

} /* end namespace Temporal */


/* because timepos_t is just a typedef here, C++ won't let this be redefined
 * (numeric_limits<uint64_t> are already implicitly defined.
 */

namespace std {

/* The utility of these two specializations of std::numeric_limits<> is not
 * clear, since both timepos_t and timecnt_t have a time domain, and comparing
 * across time domains, while possible, is expensive.
 *
 * To avoid hidden costs, it seems easier to use timepos_t::max and
 * timecnt_t::max although these have a similar fundamental problem.
 */

template<>
struct numeric_limits<Temporal::timepos_t> {
	static Temporal::timepos_t min() {
		return Temporal::timepos_t::from_superclock (0);
	}
	static Temporal::timepos_t max() {
		return Temporal::timepos_t::from_superclock (4611686018427387904); /* pow (2,62) */
	}
};

template<>
struct numeric_limits<Temporal::timecnt_t> {
	static Temporal::timecnt_t min() { return Temporal::timecnt_t::from_superclock (0); }
	static Temporal::timecnt_t max() { return Temporal::timecnt_t::from_superclock (4611686018427387904); /* pow (2,62) */ }
};

}


namespace std {
std::ostream&  operator<< (std::ostream & o, Temporal::timecnt_t const & tc);
std::ostream&  operator<< (std::ostream & o, Temporal::timepos_t const & tp);
}

#if 0
inline static bool operator< (Temporal::samplepos_t s, Temporal::timepos_t const & t) { return s < t.samples(); }
inline static bool operator< (Temporal::Beats const & b, Temporal::timepos_t const & t) { return b < t.beats(); }

inline static bool operator<= (Temporal::samplepos_t s, Temporal::timepos_t const & t) { return s <= t.samples(); }
inline static bool operator<= (Temporal::Beats const & b, Temporal::timepos_t const & t) { return b <= t.beats(); }

inline static bool operator> (Temporal::samplepos_t s, Temporal::timepos_t const & t) { return s > t.samples(); }
inline static bool operator> (Temporal::Beats const & b, Temporal::timepos_t const & t) { return b > t.beats(); }

inline static bool operator>= (Temporal::samplepos_t s, Temporal::timepos_t const & t) { return s >= t.samples(); }
inline static bool operator>= (Temporal::Beats const & b, Temporal::timepos_t const & t) { return b >= t.beats(); }

#ifdef TEMPORAL_DOMAIN_WARNING
#undef TEMPORAL_DOMAIN_WARNING
#endif

#define TEMPORAL_DOMAIN_WARNING(d) if (t.time_domain() != (d)) std::cerr << "DOMAIN CONVERSION WARNING IN COMPARATOR with t.domain = " << enum_2_string (t.time_domain()) << " not " << enum_2_string (d) << std::endl;

inline static bool operator< (Temporal::samplepos_t s, Temporal::timecnt_t const & t) { TEMPORAL_DOMAIN_WARNING (Temporal::AudioTime); return s < t.samples(); }
inline static bool operator< (Temporal::Beats const & b, Temporal::timecnt_t const & t) { TEMPORAL_DOMAIN_WARNING (Temporal::BeatTime); return b < t.beats(); }

inline static bool operator<= (Temporal::samplepos_t s, Temporal::timecnt_t const & t) { TEMPORAL_DOMAIN_WARNING (Temporal::AudioTime); return s <= t.samples(); }
inline static bool operator<= (Temporal::Beats const & b, Temporal::timecnt_t const & t) { TEMPORAL_DOMAIN_WARNING (Temporal::BeatTime); return b <= t.beats(); }

inline static bool operator> (Temporal::samplepos_t s, Temporal::timecnt_t const & t) { TEMPORAL_DOMAIN_WARNING (Temporal::AudioTime); return s > t.samples(); }
inline static bool operator> (Temporal::Beats const & b, Temporal::timecnt_t const & t) { TEMPORAL_DOMAIN_WARNING (Temporal::BeatTime); return b > t.beats(); }

inline static bool operator>= (Temporal::samplepos_t s, Temporal::timecnt_t const & t) { TEMPORAL_DOMAIN_WARNING (Temporal::AudioTime); return s >= t.samples(); }
inline static bool operator>= (Temporal::Beats const & b, Temporal::timecnt_t const & t) { TEMPORAL_DOMAIN_WARNING (Temporal::BeatTime); return b >= t.beats(); }
#endif

#undef TEMPORAL_DOMAIN_WARNING

#endif /* __libtemporal_timeline_h__ */
