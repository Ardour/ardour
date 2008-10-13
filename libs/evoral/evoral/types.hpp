/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_TYPES_HPP
#define EVORAL_TYPES_HPP

#include <stdint.h>

namespace Evoral {

/** Frame count (i.e. length of time in audio frames) */
typedef uint32_t FrameTime;

/** Time-stamp of an event */
typedef double timestamp_t;

/** Duration of time in timestamp_t units */
typedef timestamp_t timedur_t;

/** Time stamp of an event */
typedef double EventTime;

/** Time stamp of an event */
typedef double EventLength;

/** Type of an event (opaque, mapped by application) */
typedef uint32_t EventType;

} // namespace Evoral

#endif // EVORAL_TYPES_HPP
