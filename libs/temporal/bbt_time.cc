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

using namespace Temporal;

BBT_Offset::BBT_Offset (double dbeats)
{
	/* NOTE: this does not construct a BBT time in a canonical form,
	   in that beats may be a very large number, and bars will
	   always be zero. Hence ... it's a BBT_Offset
	*/

	assert (dbeats >= 0);

	bars = 0;
	beats = lrint (floor (dbeats));
	ticks = lrint (floor (Temporal::ticks_per_beat * fmod (dbeats, 1.0)));
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

	bbt = Temporal::BBT_Time (B, b, t);

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
