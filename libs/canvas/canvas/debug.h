/*
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_DEBUG_H__
#define __CANVAS_DEBUG_H__

#include <sys/time.h>
#include <map>
#include "pbd/debug.h"

#include "canvas/visibility.h"

namespace PBD {
	namespace DEBUG {
		LIBCANVAS_API extern DebugBits CanvasItems;
		LIBCANVAS_API extern DebugBits CanvasItemsDirtied;
		LIBCANVAS_API extern DebugBits CanvasEvents;
		LIBCANVAS_API extern DebugBits CanvasRender;
		LIBCANVAS_API extern DebugBits CanvasEnterLeave;
		LIBCANVAS_API extern DebugBits CanvasBox;
		LIBCANVAS_API extern DebugBits CanvasSizeAllocate;
		LIBCANVAS_API extern DebugBits CanvasTable;
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
