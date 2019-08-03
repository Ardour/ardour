/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cmath>
#include <cassert>

#include "temporal/bbt_time.h"

using namespace Timecode;

/* This defines the smallest division of a "beat".

   The number is intended to have as many integer factors as possible so that
   1/Nth divisions are integer numbers of ticks.

   1920 has many factors, though going up to 3840 gets a couple more.

   This needs to match Temporal::Beats::PPQN
*/

const double BBT_Time::ticks_per_beat = 1920.0;

BBT_Offset::BBT_Offset (double dbeats)
{
	/* NOTE: this does not construct a BBT time in a canonical form,
	   in that beats may be a very large number, and bars will
	   always be zero. Hence ... it's a BBT_Offset
	*/

	assert (dbeats >= 0);

	bars = 0;
	beats = lrint (floor (dbeats));
	ticks = lrint (floor (BBT_Time::ticks_per_beat * fmod (dbeats, 1.0)));
}
