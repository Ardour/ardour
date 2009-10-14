/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_timestamps_h__
#define __ardour_timestamps_h__

#ifdef WITH_JACK_TIMESTAMPS
#include <jack/timestamps.h>
#else
#define jack_timestamp(s)
#define jack_init_timestamps(n)
#define jack_dump_timestamps(o)
#define jack_reset_timestamps()
#endif

#endif /* __ardour_timestamps_h__ */
