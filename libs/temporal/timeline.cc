/*
    Copyright (C) 2017 Paul Davis

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

#include <cstdlib>

#include <exception>
#include <sstream>

#include "pbd/enumwriter.h"
#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/i18n.h"

#include "temporal/debug.h"
#include "temporal/timeline.h"
#include "temporal/tempo.h"

using namespace PBD;
using namespace Temporal;

struct TemporalStatistics
{
	int64_t audio_to_beats;
	int64_t audio_to_bars;
	int64_t beats_to_audio;
	int64_t beats_to_bars;
	int64_t bars_to_audio;
	int64_t bars_to_beats;

	TemporalStatistics ()
		: audio_to_beats (0)
		, audio_to_bars (0)
		, beats_to_audio (0)
		, beats_to_bars (0)
		, bars_to_audio (0)
		, bars_to_beats (0)
	{}

	void dump (std::ostream & str) {
		str << "TemporalStatistics\n"
		    << "Audio => Beats " << audio_to_beats
		    << "Audio => Bars " << audio_to_bars
		    << "Beats => Audio " << beats_to_audio
		    << "Beats => Bars " << beats_to_bars
		    << "Bars => Audio " << bars_to_audio
		    << "Bars => Beats " << bars_to_beats
		    << std::endl;
	}
};

static TemporalStatistics stats;

/* timecnt */

timecnt_t timecnt_t::_max_timecnt (timecnt_t::from_superclock (int62_t::max - 1));

timecnt_t::timecnt_t (timecnt_t const & tc, timepos_t const & pos)
	: _position (pos)
{
	if (tc.distance() < 0) {
		throw std::domain_error (X_("negative distance in timecnt constructor"));
	}

	_distance = tc.distance();
}

void
timecnt_t::set_position (timepos_t const & pos)
{
	_position = pos;
}

timecnt_t
timecnt_t::abs () const
{
	return timecnt_t (_distance.abs(), _position);
}

superclock_t
timecnt_t::compute_superclocks() const
{
	assert (_distance.flagged());
	TempoMap::SharedPtr tm (TempoMap::use());
	return tm->full_duration_at (_position, *this, AudioTime).superclocks();
}

Beats
timecnt_t::compute_beats() const
{
	assert (!_distance.flagged());
	TempoMap::SharedPtr tm (TempoMap::use());
	return tm->full_duration_at (_position, *this, BeatTime).beats();
}

timecnt_t
timecnt_t::operator*(ratio_t const & r) const
{
	const int62_t v (_distance.flagged(), int_div_round (_distance.val() * r.numerator(), r.denominator()));
	return timecnt_t (v, _position);
}

ratio_t
timecnt_t::operator/ (timecnt_t const & other) const
{
	if (time_domain() == other.time_domain()) {
		return ratio_t (distance().val(), other.distance().val());
	}

	if (time_domain() == AudioTime) {
		return ratio_t (distance().val(), other.samples());
	}

	return ratio_t (beats().to_ticks(), other.beats().to_ticks());
}

timecnt_t
timecnt_t::operator/(ratio_t const & r) const
{
	/* note: x / (N/D) => x * (D/N) => (x * D) / N */

	const int62_t v (_distance.flagged(), int_div_round (_distance.val() * r.denominator(), r.numerator()));
	return timecnt_t (v, _position);
}

timecnt_t
timecnt_t::operator% (timecnt_t const & d) const
{
	return timecnt_t (_distance % d.distance(), _position);
}

timecnt_t &
timecnt_t::operator%= (timecnt_t const & d)
{
	_distance %= d.distance();
	return *this;
}

bool
timecnt_t::string_to (std::string const & str)
{
	superclock_t s;
	Beats beats;
	char sep;

	if (isdigit (str[0])) {
		/* old school position format: we assume samples */
		std::stringstream ss (str);
		ss >> s;
		_distance = s;
		std::cerr << "deserialized timecnt from older " << str << " as " << *this << std::endl;
		return true;
	}

	std::stringstream ss (str.substr (1));

	switch (str[0]) {
	case 'a':
		ss >> s;
		_distance = int62_t (false, s);
		break;
	case 'b':
		ss >> beats;
		_distance = int62_t (true, beats.to_ticks());
		break;
	}

	/* eat separator character */

	ss >> sep;

	/* grab what's left, generate a new string and parse _position with it */

	std::string remaining;
	ss >> remaining;

	_position.string_to (remaining);

	return true;
}

std::string
timecnt_t::to_string () const
{
	std::stringstream ss;

	if (_distance.flagged()) {
		ss << 'b';
	} else {
		ss << 'a';
	}

	ss << _distance.val();

	/* add a separator. character doesn't matter as long as it will never be
	   parsed as part of a numerical value. Using '@' makes it "read
	   nicely" e.g. "3 beats at superclock 28229992292"
	*/

	ss << '@';
	ss << _position.to_string();

	return ss.str();
}

timecnt_t
timecnt_t::operator+ (timecnt_t const & other) const
{
	if (time_domain() == AudioTime) {
		if (other.time_domain() == AudioTime) {
			/* both audio, just add and use an arbitrary position */
			return timecnt_t (_distance + other.distance(), _position);
		} else {
			return timecnt_t (_distance + other.samples(), _position);
		}
	}

	return timecnt_t (beats() + other.beats(), _position);
}

timecnt_t
timecnt_t::operator- (timecnt_t const & other) const
{
	if (time_domain() == AudioTime) {
		if (other.time_domain() == AudioTime) {
			return timecnt_t (_distance - other.distance(), _position);
		} else {
			return timecnt_t (_distance - other.samples(), _position);
		}
	}

	return timecnt_t (beats() - other.beats(), _position);
}

timecnt_t &
timecnt_t::operator+= (timecnt_t const & other)
{
	if (time_domain() == AudioTime) {
		if (other.time_domain() == AudioTime) {
			_distance += other.distance();
		} else {
			_distance += other.samples();
		}
	} else {
		_distance += other.ticks ();
	}

	return *this;
}

timecnt_t
timecnt_t::operator+ (timepos_t const & other) const
{
	if (time_domain() == AudioTime) {
		if (other.time_domain() == AudioTime) {
			/* both audio, just add and use an arbitrary position */
			return timecnt_t (_distance + other.val(), _position);
		} else {
			return timecnt_t (_distance + other.samples(), _position);
		}
	}

	return timecnt_t (beats() + other.beats(), _position);
}

timecnt_t
timecnt_t::operator- (timepos_t const & other) const
{
	if (time_domain() == AudioTime) {
		if (other.time_domain() == AudioTime) {
			return timecnt_t (_distance - other.val(), _position);
		} else {
			return timecnt_t (_distance - other.samples(), _position);
		}
	}

	return timecnt_t (beats() - other.beats(), _position);
}

timecnt_t &
timecnt_t::operator-= (timecnt_t const & other)
{
	if (time_domain() == other.time_domain()) {
		_distance -= other.distance();
	} else if (time_domain() == AudioTime) {
		_distance -= other.samples();
	} else {
		_distance -= other.ticks ();
	}

	return *this;
}

timecnt_t
timecnt_t::operator- () const
{
	return timecnt_t (-_distance, _position);
}

bool
timecnt_t::expensive_lt (timecnt_t const & other) const
{
	if (!_distance.flagged()) { /* Audio */
		return _distance.val() < other.superclocks();
	}

	return Beats::ticks (_distance.val()) < other.beats ();
}

bool
timecnt_t::expensive_gt (timecnt_t const & other) const
{
	if (!_distance.flagged()) { /* Audio */
		return _distance.val() > other.superclocks();
	}

	return Beats::ticks (_distance.val()) > other.beats ();
}

bool
timecnt_t::expensive_lte (timecnt_t const & other) const
{
	if (!_distance.flagged()) { /* Audio */
		return _distance.val() <= other.superclocks();
	}

	return Beats::ticks (_distance.val()) <= other.beats ();
}

bool
timecnt_t::expensive_gte (timecnt_t const & other) const
{
	if (time_domain() == AudioTime) {
		return _distance.val() >= other.superclocks();
	}

	return Beats::ticks (_distance.val()) >= other.beats ();
}

std::ostream&
std::operator<< (std::ostream & o, timecnt_t const & tc)
{
	return o << tc.to_string();
}

/* timepos */

timepos_t::timepos_t (timecnt_t const & t)
{
	if (t.distance() < 0) {
		std::cerr << "timecnt_t has negative distance distance " << " val " << t.distance().val() << " flagged " << t.distance().flagged() << std::endl;
		throw  std::domain_error("negative value for timepos_t constructor");
	}

	v = build (t.distance().flagged(), t.distance ().val());
}

// timepos_t timepos_t::_max_timepos (Temporal::AudioTime);

timepos_t &
timepos_t::operator= (timecnt_t const & t)
{
	v = build (t.distance().flagged(), t.distance().val());
	return *this;
}

bool
timepos_t::operator< (timecnt_t const & t) const
{
	if (time_domain() == AudioTime) {
		return superclocks() < t.superclocks();
	}

	return beats() < t.beats ();
}

bool
timepos_t::operator> (timecnt_t const & t) const
{
	if (time_domain() == AudioTime) {
		return superclocks() > t.superclocks();
	}

	return beats() > t.beats ();
}

bool
timepos_t::operator<= (timecnt_t const & t) const
{
	if (time_domain() == AudioTime) {
		return superclocks() <= t.superclocks();
	}

	return beats() <= t.beats ();
}

bool
timepos_t::operator>= (timecnt_t const & t) const
{
	if (time_domain() == AudioTime) {
		return superclocks() >= t.superclocks();
	}

	return beats() >= t.beats ();
}

void
timepos_t::set_superclock (superclock_t s)
{
	v = build (false, s);
}

void
timepos_t::set_beat (Beats const & b)
{
	v = build (true, b.to_ticks());
}

superclock_t
timepos_t::_superclocks () const
{
	stats.beats_to_audio++;

	TempoMap::SharedPtr tm (TempoMap::use());
	return tm->superclock_at (beats ());
}

Temporal::Beats
timepos_t::_beats () const
{
	stats.audio_to_beats++;

	TempoMap::SharedPtr tm (TempoMap::use());
	return tm->quarters_at_superclock (v);
}

int64_t
timepos_t::_ticks () const
{
	return _beats().to_ticks();
}

timepos_t
timepos_t::operator/(ratio_t const & n) const
{
	/* this cannot make the value negative, since ratio_t is always positive */
	/* note: v / (N/D) = (v * D) / N */

	return timepos_t (is_beats(), int_div_round (val() * n.denominator(), n.numerator()));
}

timepos_t
timepos_t::operator*(ratio_t const & n) const
{
	/* this cannot make the value negative, since ratio_t is always positive */
	return timepos_t (is_beats(), int_div_round (val() * n.numerator(), n.denominator()));
}

timepos_t &
timepos_t::operator/=(ratio_t const & n)
{
	/* this cannot make the value negative, since ratio_t is always positive */
	v = build (flagged(), int_div_round (val() * n.numerator(), n.denominator()));
	return *this;
}

timepos_t &
timepos_t::operator*=(ratio_t const & n)
{
	/* this cannot make the value negative, since ratio_t is always positive */
	v = build (flagged(), int_div_round (val() * n.denominator(), n.numerator()));
	return *this;
}

timepos_t
timepos_t::expensive_add (timepos_t const & other) const
{
	/* Called when other's time domain does not match our own, requiring us
	   to call either ::beats() or ::superclocks() on other to convert it to
	   our time domain.
	*/

	assert (is_beats() != other.is_beats ());

	if (is_beats()) {
		/* we are known to use music time, so val() is in ticks */
		return timepos_t::from_ticks (val() + other.ticks());
	}

	/* we are known to use audio time, so val() is in superclocks */
	return timepos_t::from_superclock (val() + other.superclocks());
}

/* */

/* ::distance() assumes that @param other is later on the timeline than this, and
 * thus returns a positive value if this condition is satisfied.
 */

timecnt_t
timepos_t::distance (timepos_t const & other) const
{
	if (time_domain() == other.time_domain()) {
		// std::cerr << "\ncomputing distance in " << enum_2_string (time_domain()) << std::endl;
		return timecnt_t (int62_t (is_beats(), other.val() - val()), *this);
	}

	// std::cerr << "\ncomputing distance on " << enum_2_string (time_domain()) << " w/other = " << enum_2_string (other.time_domain()) << std::endl;

	return expensive_distance (other);
}

timecnt_t
timepos_t::expensive_distance (timepos_t const & other) const
{
	/* Called when other's time domain does not match our own, requiring us
	   to call either ::beats() or ::superclocks() on other to convert it to
	   our time domain.
	*/

	assert (is_beats() != other.is_beats ());

	if (is_beats()) {
		/* we are known to use beat time: val() is ticks */
		return timecnt_t::from_ticks (other.ticks() - val(), *this);
	}
	/* we known to be audio: val() is superclocks */

	// std::cerr << "other " << other << " SC = " << other.superclocks() << " vs. us @ " << val() << std::endl;
	return timecnt_t::from_superclock (other.superclocks() - val(), *this);
}

/* */

timepos_t
timepos_t::earlier (Temporal::BBT_Offset const & offset) const
{
	TempoMap::SharedPtr tm (TempoMap::use());

	if (is_superclock()) {
		return timepos_t (tm->superclock_at (tm->bbt_walk (tm->bbt_at (superclocks()), -offset)));
	}

	return timepos_t (tm->bbtwalk_to_quarters (beats(), -offset));
}


timepos_t
timepos_t::earlier (timepos_t const & other) const
{
	if (is_superclock()) {
		return timepos_t::from_superclock (val() - other.superclocks());
	}

	return timepos_t::from_ticks (val() - other.ticks());
}

timepos_t
timepos_t::earlier (timecnt_t const & distance) const
{
	if (is_superclock()) {
		return timepos_t::from_superclock (val() - distance.superclocks());
	}

	return timepos_t::from_ticks (val() - distance.ticks());
}

bool
timepos_t::expensive_lt (timepos_t const & other) const
{
	if (time_domain() == AudioTime) {
		return val() < other.superclocks();
	}

	return ticks() < other.ticks ();
}

bool
timepos_t::expensive_gt (timepos_t const & other) const
{
	if (time_domain() == AudioTime) {
		return superclocks() > other.superclocks();
	}

	return beats() > other.beats ();
}

bool
timepos_t::expensive_lte (timepos_t const & other) const
{
	if (time_domain() == AudioTime) {
		return superclocks() <= other.superclocks();
	}

	return beats() <= other.beats ();
}

bool
timepos_t::expensive_gte (timepos_t const & other) const
{
	if (time_domain() == AudioTime) {
		return superclocks() >= other.superclocks();
	}

	return beats() >= other.beats ();
}

/* */

timepos_t &
timepos_t::shift_earlier (timepos_t const & d)
{
	if (is_superclock()) {
		v = build (false, val() - d.superclocks());
	} else {
		v = build (true, val() - d.ticks());
	}

	return *this;
}

timepos_t &
timepos_t::shift_earlier (timecnt_t const & d)
{
	if (is_superclock()) {
		v = build (false, val() - d.superclocks());
	} else {
		v = build (true, val() - d.ticks());
	}

	return *this;
}

timepos_t &
timepos_t::shift_earlier (Temporal::BBT_Offset const & offset)
{
	TempoMap::SharedPtr tm (TempoMap::use());

	if (is_superclock()) {
		v = build (false, (tm->superclock_at (tm->bbt_walk (tm->bbt_at (superclocks()), -offset))));
	} else {
		v = build (true, tm->bbtwalk_to_quarters (beats(), -offset).to_ticks());
	}

	return *this;
}

/* */

timepos_t &
timepos_t::operator+= (Temporal::BBT_Offset const & offset)
{
	TempoMap::SharedPtr tm (TempoMap::use());
	if (is_beats()) {
		v = build (true, tm->bbtwalk_to_quarters (beats(), offset).to_ticks());
	} else {
		v = build (false, tm->superclock_at (tm->bbt_walk (tm->bbt_at (superclocks()), offset)));
	}

	return *this;
}

/* */

timepos_t
timepos_t::operator+(timecnt_t const & d) const
{
	if (d.time_domain() == AudioTime) {
		return operator+ (timepos_t::from_superclock (d.superclocks()));
	}

	return operator+ (timepos_t::from_ticks (d.ticks()));
}

timepos_t &
timepos_t::operator+=(timecnt_t const & d)
{
	if (d.time_domain() == AudioTime) {
		return operator+= (timepos_t::from_superclock (d.superclocks()));
	}
	return operator+= (timepos_t::from_ticks (d.ticks()));
}

/* */

timepos_t &
timepos_t::operator+=(timepos_t const & d)
{
	if (d.is_beats() == is_beats()) {

		v = build (flagged(), val() + d.val());

	} else {

		if (is_beats()) {
			v = build (false, val() + d.ticks());
		} else {
			v = build (true, val() + d.superclocks());
		}
	}

	return *this;
}


std::ostream&
std::operator<< (std::ostream & o, timepos_t const & tp)
{
	return o << tp.to_string();
}

std::string
timepos_t::to_string () const
{
	if (is_beats()) {
		return string_compose ("b%1", val());
	}

	return string_compose ("a%1", val());
}

bool
timepos_t::string_to (std::string const & str)
{
	using std::string;
	using std::cerr;
	using std::endl;

	superclock_t s;
	Beats beats;

	if (isdigit (str[0])) {
		/* old school position format: we assume samples */
		std::stringstream ss (str);
		ss >> s;
		set_superclock (s);
		cerr << "deserialized timepos from older " << str << " as " << *this << endl;
		return true;
	}

	std::stringstream ss (str.substr (1));

	switch (str[0]) {
	case 'a':
		ss >> s;
		set_superclock (s);
		cerr << "deserialized timepos from " << str << " as " << *this << endl;
		return true;
	case 'b':
		ss >> beats;
		set_beat (beats);
		cerr << "deserialized timepos from " << str << " as " << *this << endl;
		return true;
	}

	std::cerr << "Unknown timepos string representation \"" << str << "\"" << std::endl;

	return false;
}
