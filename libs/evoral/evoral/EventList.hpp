/* This file is part of Evoral.
 * Copyright (C) 2009 Paul Davis
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

#ifndef EVORAL_EVENT_LIST_HPP
#define EVORAL_EVENT_LIST_HPP

#include <list>

#include "evoral/visibility.h"
#include "evoral/EventSink.hpp"
#include "evoral/types.hpp"
#include "evoral/Event.hpp"

namespace Evoral {


/** A list of events (generic time-stamped binary "blobs").
 *
 * Used when we need an unsorted list of Events that is also an EventSink. Absolutely nothing more.
 */
template<typename Time>
class LIBEVORAL_API EventList : public std::list<Evoral::Event<Time> *>, public Evoral::EventSink<Time> {
public:
	EventList() {}

	uint32_t write(Time  time, EventType  type, uint32_t  size, const uint8_t* buf) {
		this->push_back(new Evoral::Event<Time>(
			          type, time, size, const_cast<uint8_t*>(buf), true)); // Event copies buffer
		return size;
	}
};


} // namespace Evoral

#endif // EVORAL_EVENT_LIST_HPP
