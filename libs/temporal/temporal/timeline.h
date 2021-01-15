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
	timepos_t (TimeDomain d) : int62_t (d != AudioTime, 0) {}

	/* for now (Sept2020) do not allow implicit type conversions */

	explicit timepos_t (samplepos_t s) : int62_t (false, samples_to_superclock (s, TEMPORAL_SAMPLE_RATE)) {}
	explicit timepos_t (Temporal::Beats const & b) : int62_t (true, b.to_ticks()) {}

	explicit timepos_t (timecnt_t const &); /* will throw() if val is negative */

	/* superclock_t and samplepos_t are the same underlying primitive type,
	 * which means we cannot use polymorphism to differentiate them. But it
	 * turns out that we more or less never construct timepos_t from an
	 * integer representing superclocks. So, there's a normal constructor
	 * for the samples case above, and ::from_superclock() here.
	 */
	static timepos_t from_superclock (superclock_t s)  { return timepos_t (false, s); }
	static timepos_t from_ticks (int64_t t)  { return timepos_t (true, t); }

	static timepos_t zero (bool is_beats) { return timepos_t (is_beats, 0); }

	bool is_beats() const { return flagged(); }
	bool is_superclock() const { return !flagged(); }

	bool positive () const { return val() > 0; }
	bool negative () const { return val() < 0; }
	bool zero ()     const { return val() == 0; }

	Temporal::TimeDomain time_domain () const { if (flagged()) return Temporal::BeatTime; return Temporal::AudioTime; }
	void set_time_domain (Temporal::TimeDomain);

	superclock_t superclocks() const { if (is_superclock()) return val(); return _superclocks (); }
	int64_t      samples() const { return superclock_to_samples (superclocks(), TEMPORAL_SAMPLE_RATE); }
	int64_t      ticks() const { if (is_beats()) return val(); return _ticks (); }
	Beats        beats() const { if (is_beats()) return Beats::ticks (val()); return _beats (); }

	timepos_t & operator= (timecnt_t const & t); /* will throw() if val is negative */

	timepos_t operator-() const { return timepos_t (int62_t::operator-()); }

	/* if both values are zero, the time domain doesn't matter */
	bool operator== (timepos_t const & other) const { return (val() == 0 && other.val() == 0) || (v == other.v); }
	bool operator!= (timepos_t const & other) const { return (val() != 0 || other.val() != 0) && (v != other.v); }


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

	/* donn't provide operator+(samplepos_t) or operator+(superclock_t)
	 * because the compiler can't disambiguate them and neither can we.
	 * to add such types, create a timepo_t and then add that.
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
	timecnt_t distance (timepos_t const & p) const;

	/* computes a new position value that is @param d earlier than this */
	timepos_t earlier (timepos_t const & d) const; /* treat d as distance measured from timeline origin */
	timepos_t earlier (timecnt_t const & d) const;

	timepos_t earlier (BBT_Offset const & d) const;

	/* like ::earlier() but changes this. loosely equivalent to operator-= */
	timepos_t & shift_earlier (timepos_t const & d);
	timepos_t & shift_earlier (timecnt_t const & d);

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

	timepos_t & operator+=(Temporal::BBT_Offset const &);

	timepos_t   operator% (timecnt_t const &) const;
	timepos_t & operator%=(timecnt_t const &);

	/* Although multiplication and division of positions seems unusual,
	 * these are used in Evoral::Curve when scaling a list of timed events
	 * along the x (time) axis.
	 */

	timepos_t   operator/  (ratio_t const & n) const;
	timepos_t   operator*  (ratio_t const & n) const;
	timepos_t & operator/= (ratio_t const & n);
	timepos_t & operator*= (ratio_t const & n);

	bool operator<  (samplepos_t s) { return samples() < s; }
	bool operator<  (Temporal::Beats const & b) { return beats() < b; }
	bool operator<= (samplepos_t s) { return samples() <= s; }
	bool operator<= (Temporal::Beats const & b) { return beats() <= b; }
	bool operator>  (samplepos_t s) { return samples() > s; }
	bool operator>  (Temporal::Beats const & b) { return beats() > b; }
	bool operator>= (samplepos_t s) { return samples() >= s; }
	bool operator>= (Temporal::Beats const & b) { return beats() >= b; }
	bool operator== (samplepos_t s) { return samples() == s; }
	bool operator== (Temporal::Beats const & b) { return beats() == b; }
	bool operator!= (samplepos_t s) { return samples() != s; }
	bool operator!= (Temporal::Beats const & b) { return beats() != b; }

	bool string_to (std::string const & str);
	std::string to_string () const;

	static timepos_t max (TimeDomain td) { return timepos_t (td != AudioTime, int62_t::max); }
	static timepos_t smallest_step (TimeDomain td) { return timepos_t (td != AudioTime, 1); }

  private:
	/* special private constructor for use when constructing timepos_t as a
	   return value using arithmetic ops
	*/
	explicit timepos_t (bool b, int64_t v) : int62_t (b, v) {}
	explicit timepos_t (int62_t const & v) : int62_t (v) {}

	/* these can only be called after verifying that the time domain does
	 * not match the relevant one i.e. call _beats() to get a Beats value
	 * when this is using the audio time doamin
	 */

	/* these three methods are to be called ONLY when we have already that
	 * the time domain of this timepos_t does not match the desired return
	 * type, and so we will need to go to the tempo map to convert
	 * between domains, which could be expensive.
	 */

	superclock_t _superclocks() const;
	int64_t      _ticks() const;
	Beats        _beats() const;

	bool expensive_lt (timepos_t const &) const;
	bool expensive_lte (timepos_t const &) const;
	bool expensive_gt (timepos_t const &) const;
	bool expensive_gte(timepos_t const &) const;

	bool expensive_lt (timecnt_t const &) const;
	bool expensive_lte (timecnt_t const &) const;
	bool expensive_gt (timecnt_t const &) const;
	bool expensive_gte(timecnt_t const &) const;

	/* used to compute stuff when time domains do not match */

	timecnt_t expensive_distance (timepos_t const & p) const;
	timepos_t expensive_add (timepos_t const & s) const;

	int62_t operator- (int62_t) const { assert (0); }
	int62_t operator- (int64_t) const { assert (0); }

	using int62_t::operator int64_t;
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
	timecnt_t () : _distance (false, 0), _position (AudioTime) {}
	timecnt_t (TimeDomain td) : _distance (td != AudioTime, 0), _position (td) {}
	timecnt_t (timecnt_t const &other) : _distance (other.distance()), _position (other.position()) {}

	/* construct from sample count (position doesn't matter due to linear nature * of audio time */
	explicit timecnt_t (samplepos_t s, timepos_t const & pos) : _distance (int62_t (false, samples_to_superclock (s, TEMPORAL_SAMPLE_RATE))), _position (pos) {}
	explicit timecnt_t (samplepos_t s) : _distance (int62_t (false, samples_to_superclock (s, TEMPORAL_SAMPLE_RATE))), _position (AudioTime) {}

	/* construct from timeline types */
	explicit timecnt_t (timepos_t const & d) : _distance (d), _position (timepos_t::zero (d.flagged())) {}
	explicit timecnt_t (timepos_t const & d, timepos_t const & p) : _distance (d), _position (p) { }
	explicit timecnt_t (timecnt_t const &, timepos_t const & pos);

	/* construct from int62_t (which will be flagged or not) and timepos_t */
	explicit timecnt_t (int62_t d, timepos_t p) : _distance (d), _position (p) {}

	/* construct from beats */
	explicit timecnt_t (Temporal::Beats const & b, timepos_t const & pos) :  _distance (true, b.to_ticks()), _position (pos) {}

	static timecnt_t zero (TimeDomain td) { return timecnt_t (timepos_t::zero (td), timepos_t::zero (td)); }

	/* superclock_t and samplepos_t are the same underlying primitive type,
	 * See comments in timepos_t above.
	 */
	static timecnt_t from_superclock (superclock_t s, timepos_t const & pos) { return timecnt_t (int62_t (false, s), pos); }
	static timecnt_t from_ticks (int64_t ticks, timepos_t const & pos) { return timecnt_t (int62_t (true, ticks), pos); }

	/* Construct from just a distance value - position is assumed to be zero */
	explicit timecnt_t (Temporal::Beats const & b) :  _distance (true, b.to_ticks()), _position (Beats()) {}
	static timecnt_t from_superclock (superclock_t s) { return timecnt_t (int62_t (false, s), timepos_t::from_superclock (0)); }
	static timecnt_t from_samples (samplepos_t s) { return timecnt_t (int62_t (false, samples_to_superclock (s, TEMPORAL_SAMPLE_RATE)), timepos_t::from_superclock (0)); }
	static timecnt_t from_ticks (int64_t ticks) { return timecnt_t (int62_t (true, ticks), timepos_t::from_ticks (0)); }

	int64_t magnitude() const { return _distance.val(); }
	int62_t const & distance() const { return _distance; }
	timepos_t const & position() const { return _position; }
	timepos_t const & origin() const { return _position; } /* alias */
	void set_position (timepos_t const &pos);

	bool positive() const { return _distance.val() > 0; }
	bool negative() const {return _distance.val() < 0; }
	bool zero() const { return _distance.val() == 0; }

	static timecnt_t const & max() { return _max_timecnt; }
	static timecnt_t max (Temporal::TimeDomain td) { return timecnt_t (timepos_t::max (td)); }

	timecnt_t abs() const;

	Temporal::TimeDomain time_domain () const { return _distance.flagged() ? BeatTime : AudioTime; }

	superclock_t    superclocks() const { if (!_distance.flagged()) return _distance.val(); return compute_superclocks(); }
	int64_t         samples() const { return superclock_to_samples (superclocks(), TEMPORAL_SAMPLE_RATE); }
	Temporal::Beats beats  () const { if (_distance.flagged()) return Beats::ticks (_distance.val()); return compute_beats(); }
	int64_t         ticks  () const { return beats().to_ticks(); }

	timecnt_t & operator= (Temporal::Beats const & b) { _distance = int62_t (true, b.to_ticks()); return *this; }

	/* return a timecnt_t that is the next/previous (earlier/later) possible position given
	 * this one
	 */
	timecnt_t operator++ () { _distance += 1; return *this; }
	timecnt_t operator-- () { _distance -= 1; return *this; }

	timecnt_t operator*(ratio_t const &) const;
	timecnt_t operator/(ratio_t const &) const;

	ratio_t operator/ (timecnt_t const &) const;

	timecnt_t operator-() const;
	timecnt_t operator- (timecnt_t const & t) const;
	timecnt_t operator+ (timecnt_t const & t) const;

	timecnt_t operator- (timepos_t const & t) const;
	timecnt_t operator+ (timepos_t const & t) const;

	timecnt_t & operator-= (timecnt_t const & t);
	timecnt_t & operator+= (timecnt_t const & t);

	timecnt_t decrement () const { return timecnt_t (_distance - 1, _position); }

	//timecnt_t operator- (timepos_t const & t) const;
	//timecnt_t operator+ (timepos_t const & t) const;
	//timecnt_t & operator-= (timepos_t);
	//timecnt_t & operator+= (timepos_t);

	bool operator> (timecnt_t const & other) const { if (_distance.flagged() == other.distance().flagged()) return _distance > other.distance (); else return expensive_gt (other); }
	bool operator>= (timecnt_t const & other) const { if (_distance.flagged() == other.distance().flagged()) return _distance >= other.distance(); else return expensive_gte (other); }
	bool operator< (timecnt_t const & other) const { if (_distance.flagged() == other.distance().flagged()) return _distance < other.distance(); else return expensive_lt (other); }
	bool operator<= (timecnt_t const & other) const { if (_distance.flagged() == other.distance().flagged()) return _distance <= other.distance(); else return expensive_gte (other); }

	timecnt_t & operator= (timecnt_t const & other) {
		if (this != &other) {
			_distance = other.distance();
			_position = other.position();
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

	bool expensive_lt (timecnt_t const & other) const;
	bool expensive_lte (timecnt_t const & other) const;
	bool expensive_gt (timecnt_t const & other) const;
	bool expensive_gte (timecnt_t const & other) const;
};

} /* end namespace Temporal */


namespace std {
std::ostream&  operator<< (std::ostream & o, Temporal::timecnt_t const & tc);
std::ostream&  operator>> (std::istream & o, Temporal::timecnt_t const & tc);
std::ostream&  operator<< (std::ostream & o, Temporal::timepos_t const & tp);
std::ostream&  operator>> (std::istream & o, Temporal::timepos_t const & tp);
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
