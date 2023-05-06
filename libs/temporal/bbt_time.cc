/*
  Copyright (C) 2002-2010 Paul Davis

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software Foundation,
  Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>
#include <cassert>

#include "temporal/bbt_time.h"
#include "temporal/bbt_argument.h"

using namespace Temporal;

int64_t
BBT_Time::as_integer () const
{
	/* up to 256 beats in a bar, 4095 ticks in a beat,
	   and whatever is left for bars (a lot!)
	*/
	return (((int64_t) bars)<<20)|(beats<<12)|ticks;
}

BBT_Time
BBT_Time::from_integer (int64_t v)
{
	int32_t B = v>>20;
	int32_t b = (v>>12) & 0xff;
	int32_t t= v & 0xfff;
	return BBT_Time (B, b, t);
}

BBT_Time
BBT_Time::round_up_to_bar() const
{
	if (ticks == 0 && beats == 1) {
		return *this;
	}
	BBT_Time b = round_up_to_beat ();
	if (b.beats > 1) {
		b.bars += 1;
		b.beats = 1;
	}
	return b;
}

BBT_Offset::BBT_Offset (double dbeats)
{
	/* NOTE: this does not construct a BBT time in a canonical form,
	   in that beats may be a very large number, and bars will
	   always be zero. Hence ... it's a BBT_Offset
	*/

	assert (dbeats >= 0);

	bars = 0;
	beats = (int32_t) lrint (floor (dbeats));
	ticks = (int32_t) lrint (floor (Temporal::ticks_per_beat * fmod (dbeats, 1.0)));
}

std::ostream&
std::operator<< (std::ostream& o, Temporal::BBT_Time const & bbt)
{
	o << bbt.bars << '|' << bbt.beats << '|' << bbt.ticks;
	return o;
}

std::ostream&
std::operator<< (std::ostream& o, const Temporal::BBT_Offset& bbt)
{
	o << bbt.bars << '|' << bbt.beats << '|' << bbt.ticks;
	return o;
}

std::istream&
std::operator>>(std::istream& i, Temporal::BBT_Offset& bbt)
{
	int32_t B, b, t;
	char skip_pipe_char;

	i >> B;
	i >> skip_pipe_char;
	i >> b;
	i >> skip_pipe_char;
	i >> t;

	bbt = Temporal::BBT_Offset (B, b, t);

	return i;
}

std::istream&
std::operator>>(std::istream& i, Temporal::BBT_Time& bbt)
{
	int32_t B, b, t;
	char skip_pipe_char;

	i >> B;
	i >> skip_pipe_char;
	i >> b;
	i >> skip_pipe_char;
	i >> t;

	bbt = Temporal::BBT_Time (B, b, t);

	return i;
}

/* define this here to avoid adding another .cc file for just this operator */

std::ostream&
std::operator<< (std::ostream& o, Temporal::BBT_Argument const & bbt)
{
	o << '@' << bbt.reference() << ':' << bbt.bars << '|' << bbt.beats << '|' << bbt.ticks;
	return o;
}

