/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#ifndef __CANVAS_DEBUG_H__
#define __CANVAS_DEBUG_H__

#include <sys/time.h>
#include <map>
#include "pbd/debug.h"

#include "canvas/visibility.h"

namespace PBD {
	namespace DEBUG {
		LIBCANVAS_API extern uint64_t CanvasItems;
		LIBCANVAS_API extern uint64_t CanvasItemsDirtied;
		LIBCANVAS_API extern uint64_t CanvasEvents;
		LIBCANVAS_API extern uint64_t CanvasRender;
	}
}

#ifdef CANVAS_DEBUG
#define CANVAS_DEBUG_NAME(i, n) i->name = n;
#else
#define CANVAS_DEBUG_NAME(i, n) /* empty */
#endif

namespace ArdourCanvas {
	LIBCANVAS_API extern struct timeval epoch;
	LIBCANVAS_API extern std::map<std::string, struct timeval> last_time;
	LIBCANVAS_API extern void checkpoint (std::string, std::string);
	LIBCANVAS_API extern void set_epoch ();
	LIBCANVAS_API extern const char* event_type_string (int event_type);
	LIBCANVAS_API extern int render_count;
	LIBCANVAS_API extern int render_depth;
	LIBCANVAS_API extern int dump_depth;
}

#endif
