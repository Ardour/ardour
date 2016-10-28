/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2016 Paul Davis
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

#ifndef EVORAL_SEQUENCER_HPP
#define EVORAL_SEQUENCER_HPP

#include "evoral/visibility.h"
#include "evoral/Sequence.hpp"

namespace Evoral {

template<typename Time>
class LIBEVORAL_API Sequencer : public Sequence<Time> {
public:
	Sequencer(const TypeMap& type_map);
	Sequencer(const Sequencer<Time>& other);

	event_id_t insert_note (uint8_t pitch, uint8_t vel_on, uint8_t vel_off, uint8_t channel, Time time, Time duration);
	void remove_note (uint8_t pitch, Time time);
};


} // namespace Evoral

#endif // EVORAL_SEQUENCER_HPP

