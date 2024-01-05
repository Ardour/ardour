#include "ardour/slice.h"

using namespace ARDOUR;
using namespace Temporal;

Slice::Slice (timepos_t const & s, timecnt_t const & l)
	: _start (Properties::start, s)
	, _length (Properties::length, l)
	, _last_length (l)
{
}

Slice::Slice (Slice const & other)
	: _start (Properties::start, other._start)
	, _length (Properties::length, other._length)
	, _last_length (other._last_length)
{
}

void
Slice::set_position (timepos_t const & pos)
{
	_length = timecnt_t (_length.val().distance(), pos);
	_last_length = _length;
}

void
Slice::set_length (timecnt_t const & len)
{
	_last_length = _length;
	_length = timecnt_t (len.distance(), _length.val().position());
}

timepos_t
Slice::source_position () const
{
	/* this is the position of the start of the source, in absolute time */
	return position().earlier (_start.val());
}

Temporal::Beats
Slice::region_distance_to_region_beats (timecnt_t const & region_relative_offset) const
{
	return timecnt_t (region_relative_offset, position()).beats ();
}

Temporal::Beats
Slice::source_beats_to_absolute_beats (Temporal::Beats beats) const
{
	/* since the return type must be beats, force source_position() to
	   beats before adding, rather than after.
	*/
	return source_position().beats() + beats;
}

Temporal::Beats
Slice::absolute_time_to_region_beats(timepos_t const & b) const
{
	 return (position().distance (b)).beats () + start().beats();
}

Temporal::timepos_t
Slice::absolute_time_to_region_time (timepos_t const & t) const
{
	return start() + position().distance (t);
}

Temporal::timepos_t
Slice::region_beats_to_absolute_time (Temporal::Beats beats) const
{
	return position() + timepos_t (beats);
}

Temporal::timepos_t
Slice::source_beats_to_absolute_time (Temporal::Beats beats) const
{
	/* return the time corresponding to `beats' relative to the start of
	   the source. The start of the source is an implied position given by
	   region->position - region->start aka ::source_position()
	*/
	return source_position() + timepos_t (beats);
}

/** Calculate  (time - source_position) in Beats
 *
 * Measure the distance between the absolute time and the position of
 * the source start, in beats. The result is positive if time is later
 * than source position.
 *
 * @param p is an absolute time
 * @returns time offset from p to the region's source position as the origin in Beat units
 */
Temporal::Beats
Slice::absolute_time_to_source_beats(timepos_t const& p) const
{
	return source_position().distance (p).beats();
}

/** Calculate (pos - source-position)
 *
 * @param p is an absolute time
 * @returns time offset from p to the region's source position as the origin.
 */
timecnt_t
Slice::source_relative_position (timepos_t const & p) const
{
	return source_position().distance (p);
}

/** Calculate (p - region-position)
 *
 * @param p is an absolute time
 * @returns the time offset using the region (timeline) position as origin
 */
timecnt_t
Slice::region_relative_position (timepos_t const & p) const
{
	return position().distance (p);
}


Temporal::TimeDomain
Slice::position_time_domain() const
{
	return position().time_domain();
}

timepos_t
Slice::end() const
{
	/* one day we might want to enforce _position, _start and _length (or
	   some combination thereof) all being in the same time domain.
	*/
	return _length.val().end();
}


void
Slice::set_start_internal (timepos_t const & s)
{
	_start = s;
}

void
Slice::set_length_internal (timecnt_t const & len)
{
	/* maintain position value of both _last_length and _length.
	 *
	 * This is very important: set_length() can only be used to the length
	 * component of _length, and set_position() can only be used to set the
	 * position component.
	 */

	_last_length = timecnt_t (_length.val().distance(), _last_length.position());
	_length = timecnt_t (len.distance(), _length.val().position());
}

void
Slice::set_position_internal (timepos_t const & pos)
{
	if (position() == pos) {
		return;
	}

	/* We emit a change of Properties::length even if the position hasn't changed
	 * (see Slice::set_position), so we must always set this up so that
	 * e.g. Playlist::notify_region_moved doesn't use an out-of-date last_position.
	 *
	 * maintain length value of both _last_length and _length.
	 *
	 * This is very important: set_length() can only be used to the length
	 * component of _length, and set_position() can only be used to set the
	 * position component.
	 */

	_last_length.set_position (position());
	_length = timecnt_t (_length.val().distance(), pos);

	/* check that the new _position wouldn't make the current
	 * length impossible - if so, change the length.
	 *
	 * XXX is this the right thing to do?
	 */
	if (timepos_t::max (_length.val().time_domain()).earlier (_length) < position()) {
		_last_length = _length;
		_length = position().distance (timepos_t::max (position().time_domain()));
	}
}

timepos_t
Slice::earliest_possible_position () const
{
	if (start() > timecnt_t (position(), timepos_t())) {
		return timepos_t::from_superclock (0);
	} else {
		return source_position();
	}
}

