/*
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2013 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_automatable_sequence_h__
#define __ardour_automatable_sequence_h__

#include "evoral/Sequence.h"
#include "ardour/automatable.h"
#include "ardour/event_type_map.h"

namespace ARDOUR {

/** Contains notes and controllers */
template<typename T>
class /*LIBARDOUR_API*/ AutomatableSequence : public Automatable, public Evoral::Sequence<T> {
public:
	AutomatableSequence(Session& s)
		: Evoral::ControlSet()
		, Automatable(s)
		, Evoral::Sequence<T>(EventTypeMap::instance())
	{}

	AutomatableSequence(const AutomatableSequence<T>& other)
		: Evoral::ControlSet(other)
		, Automatable(other._a_session)
		, Evoral::Sequence<T>(other)
	{}

};

} // namespace ARDOUR

#endif /* __ardour_automatable_sequence_h__ */

