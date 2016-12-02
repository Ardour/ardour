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

#include <glib.h>

#include "evoral/Event.hpp"
#include "evoral/Beats.hpp"
#include "evoral/Sequence.hpp"

namespace Evoral {

static event_id_t _event_id_counter = 0;

template<typename Time>
EventPointer<Time>::~EventPointer ()
{
	std::cerr << "::~EventPointer\n";
}

template<typename Time>
ManagedEvent<Time>::~ManagedEvent ()
{
	std::cerr << "::~ManagedEvent\n";
}

event_id_t
event_id_counter()
{
	return g_atomic_int_get (&_event_id_counter);
}

void
init_event_id_counter(event_id_t n)
{
	g_atomic_int_set (&_event_id_counter, n);
}

event_id_t
next_event_id ()
{
	/* TODO: handle 31bit overflow , event_id_t is an int32_t,
	 * and libsmf only supports loading uint32_t vlq's, see smf_extract_vlq()
	 *
	 * event-IDs only have to be unique per .mid file.
	 * Previously (Ardour 4.2ish) Ardour re-generated those IDs when loading the
	 * file but that lead to .mid files being modified on every load/save.
	 *
	 * current user-record: is event-counter="276390506" (just abov 2^28)
	 */
	return g_atomic_int_add (&_event_id_counter, 1);
}

/* each type of EventPointer<Time> needs it own pool
 */

template<typename Time>
EventPool EventPointer<Time>::pool ("event pointer");

template<typename Time>
EventPool ManagedEvent<Time>::default_event_pool ("default event");

template class Event<Evoral::Beats>;
template class Event<double>;
template class Event<int64_t>; /* framepos_t in Ardour */

template class ManagedEvent<Evoral::Beats>;
template class ManagedEvent<double>;
template class ManagedEvent<int64_t>; /* framepos_t in Ardour */

template class EventPointer<Evoral::Beats>;
template class EventPointer<int64_t>; /* framepos_t in Ardour */

} // namespace Evoral
