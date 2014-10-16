/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
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

#ifndef EVORAL_EVENT_SINK_HPP
#define EVORAL_EVENT_SINK_HPP

#include "evoral/visibility.h"
#include "evoral/types.hpp"

namespace Evoral {

/** Pure virtual base for anything you can write events to.
 */
template<typename Time>
class /*LIBEVORAL_API*/ EventSink {
public:
	virtual ~EventSink() {}
	virtual uint32_t write(Time time, EventType type, uint32_t size, const uint8_t* buf) = 0;
};


} // namespace Evoral

#endif // EVORAL_EVENT_SINK_HPP

