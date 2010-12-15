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

#include "timecode/bbt_time.h"

using namespace Timecode;

const double BBT_Time::ticks_per_beat = 1920.0;

BBT_Time::BBT_Time (double dbeats)
{
        /* NOTE: this does not construct a BBT time in a canonical form,
           in that beats may be a very large number, and bars will
           always be zero.
        */

        bars = 0;
        beats = rint (floor (dbeats));
        ticks = rint (floor (BBT_Time::ticks_per_beat * fmod (dbeats, 1.0)));
}
