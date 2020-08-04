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

timecnt_t timecnt_t::_max_timecnt (max_samplepos, timepos_t());

timecnt_t::timecnt_t (timecnt_t const & tc, timepos_t const & pos)
	: _position (pos)
{
	if (tc.distance() < 0) {
		throw std::domain_error (X_("negative distance in timecnt constructor"));
	}

	_distance = tc.distance();

	assert (_position.is_beats() == _distance.flagged());
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
	TempoMap::SharedPtr tm (TempoMap::fetch());
	return tm->full_duration_at (_position, *this, AudioTime).superclocks();
}

Beats
timecnt_t::compute_beats() const
{
	assert (!_distance.flagged());
	TempoMap::SharedPtr tm (TempoMap::fetch());
	return tm->full_duration_at (_position, *this, BeatTime).beats();
}

timecnt_t
timecnt_t::operator*(ratio_t const & r) const
{
	return timecnt_t (int_div_round (_distance.val() * r.numerator(), r.denominator()), _position);
}

timecnt_t
timecnt_t::operator/(ratio_t const & r) const
{
	return timecnt_t (int_div_round (_distance.val() * r.denominator(), r.numerator()), _position);
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
		_distance = s;
		break;
	case 'b':
		ss >> beats;
		_distance = beats;
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

/* timepos */

timepos_t::timepos_t (timecnt_t const & t)
{
	if (t.distance() < 0) {
		throw  std::domain_error("negative value for timepos_t constructor");
	}

	v = t.distance ();
}

// timepos_t timepos_t::_max_timepos (Temporal::AudioTime);

timepos_t &
timepos_t::operator= (timecnt_t const & t)
{
	v = t.distance();
	return *this;
}

void
timepos_t::set_superclock (superclock_t s)
{
	v = s;
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

	TempoMap::SharedPtr tm (TempoMap::fetch());
	return tm->superclock_at (beats ());
}

Temporal::Beats
timepos_t::_beats () const
{
	stats.audio_to_beats++;

	TempoMap::SharedPtr tm (TempoMap::fetch());
	return tm->quarter_note_at (v);
}

int64_t
timepos_t::_ticks () const
{
	return _beats().to_ticks();
}

timepos_t
timepos_t::operator/(ratio_t const & n) const
{
	assert (n >= 0); /* do not allow a position to become negative via division */

	/* note: v / (N/D) = (v * D) / N */

	return timepos_t (is_beats(), int_div_round (val() * n.denominator(), n.numerator()));
}

timepos_t
timepos_t::operator*(ratio_t const & n) const
{
	assert (n >= 0); /* do not allow a position to become negative via multiplication */
	return timepos_t (is_beats(), int_div_round (val() * n.numerator(), n.denominator()));
}

timepos_t &
timepos_t::operator/=(ratio_t const & n)
{
	assert (n >= 0); /* do not allow a position to become negative via division */
	v = build (flagged(), int_div_round (val() * n.numerator(), n.denominator()));
	return *this;
}

timepos_t &
timepos_t::operator*=(ratio_t const & n)
{
	assert (n >= 0); /* do not allow a position to become negative via multiplication */
	v = build (flagged(), int_div_round (val() * n.denominator(), n.numerator()));
	return *this;
}

timepos_t
timepos_t::expensive_add (Beats const & b) const
{
	assert (is_beats());
	return timepos_t (beats() + b);
}

timepos_t
timepos_t::expensive_add (superclock_t sc) const
{
	return timepos_t (val() + sc);
}

/* */

/* ::distance() assumes that @param d is later on the timeline than this, and
 * thus returns a positive value if this condition is satisfied.
 */

timecnt_t
timepos_t::distance (superclock_t s) const
{
	if (is_superclock()) {
		return timecnt_t (s - val(), *this);
	}

	return expensive_distance (s);
}

timecnt_t
timepos_t::distance (Beats const & b) const
{
	if (is_beats()) {
		return timecnt_t ((b - _beats()).to_ticks(), *this);
	}

	return expensive_distance (b);
}

timecnt_t
timepos_t::distance (timepos_t const & d) const
{
	if (d.is_beats()) {
		return distance (d._beats());
	}

	return distance (d._superclocks());
}

timecnt_t
timepos_t::expensive_distance (superclock_t s) const
{
	if (is_superclock() && (val() < s)) {
		PBD::warning << string_compose (_("programming warning: sample arithmetic will generate negative sample time (%1 - %2)"), superclocks(), s) << endmsg;
	}

	return timecnt_t (s - superclocks(), *this);
}

timecnt_t
timepos_t::expensive_distance (Temporal::Beats const & b) const
{
	return timecnt_t (b - beats(), *this);
}

/* */

timepos_t
timepos_t:: earlier (superclock_t s) const
{
	superclock_t sc;

	if (is_beats()) {
		TempoMap::SharedPtr tm (TempoMap::fetch());
		sc = tm->superclock_at (beats());
	} else {
		sc = val();
	}

	if (sc < s) {
		PBD::warning << string_compose (_("programming warning: sample arithmetic will generate negative sample time (%1 - %2)"), superclocks(), s) << endmsg;
	}

	return timepos_t (sc - s);
}

timepos_t
timepos_t::earlier (Temporal::Beats const & b) const
{
	Beats bb;

	if (is_superclock()) {
		TempoMap::SharedPtr tm (TempoMap::fetch());
		bb = tm->quarter_note_at (superclocks());
	} else {
		bb = beats ();
	}

	return timepos_t (bb - b);
}

timepos_t
timepos_t::earlier (timepos_t const & other) const
{
	if (other.is_superclock()) {
		return earlier (other.superclocks());
	}

	return earlier (other.beats());
}

timepos_t
timepos_t::earlier (timecnt_t const & distance) const
{
	if (distance.time_domain() == AudioTime) {
		return earlier (distance.superclocks());
	}
	return earlier (distance.beats());
}

/* */

timepos_t &
timepos_t::shift_earlier (timecnt_t const & d)
{
	if (d.time_domain() == AudioTime) {
		return shift_earlier (d.superclocks());
	}

	return shift_earlier (d.beats());
}

timepos_t &
timepos_t:: shift_earlier (superclock_t s)
{
	superclock_t sc;

	if (is_beats ()) {
		TempoMap::SharedPtr tm (TempoMap::fetch());
		sc = tm->superclock_at (beats());
	} else {
		sc = val();
	}

	v = sc - s;

	return *this;
}

timepos_t &
timepos_t::shift_earlier (Temporal::Beats const & b)
{
	Beats bb;

	if (is_superclock()) {
		TempoMap::SharedPtr tm (TempoMap::fetch());
		bb = tm->quarter_note_at (val());
	} else {
		bb = beats ();
	}

	v = (bb - b).to_ticks();

	return *this;
}

/* */

timepos_t &
timepos_t:: operator+= (superclock_t s)
{
	if (is_superclock()) {
		v += s;
	} else {
		TempoMap::SharedPtr tm (TempoMap::fetch());
		v = build (true, tm->scwalk_to_quarters (beats(), s).to_ticks());
	}

	return *this;
}

timepos_t &
timepos_t::operator+=(Temporal::Beats const & b)
{
	if (is_beats()) {
		v += build (true, b.to_ticks());
	} else {
		TempoMap::SharedPtr tm (TempoMap::fetch());
		v = tm->superclock_plus_quarters_as_superclock (val(), b);
	}

	return *this;
}

/* */

timepos_t
timepos_t::operator+(timecnt_t const & d) const
{
	if (d.time_domain() == AudioTime) {
		return operator+ (d.superclocks());
	}

	return operator+ (d.beats());
}

timepos_t &
timepos_t::operator+=(timecnt_t const & d)
{
	if (d.time_domain() == AudioTime) {
		return operator+= (d.superclocks());
	}
	return operator+= (d.beats());
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
