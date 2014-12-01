/* This file is part of Evoral.
 * Copyright (C) 2000-2014 Paul Davis
 * Author: David Robillard
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

#include "evoral/ParameterDescriptor.hpp"

namespace Evoral {

ParameterDescriptor::ParameterDescriptor()
	: key((uint32_t)-1)
	, datatype(Variant::NOTHING)
	, unit(NONE)
	, normal(0)
	, lower(0)
	, upper(0)
	, step(0)
	, smallstep(0)
	, largestep(0)
	, integer_step(false)
	, toggled(false)
	, logarithmic(false)
	, sr_dependent(false)
	, min_unbound(0)
	, max_unbound(0)
	, enumeration(false)
{}

/* Set step, smallstep, and largestep, based on current description */
void
ParameterDescriptor::update_steps()
{
	if (unit == ParameterDescriptor::MIDI_NOTE) {
		step      = smallstep = 1;  // semitone
		largestep = 12;             // octave
	} else if (integer_step) {
		const float delta = upper - lower;

		smallstep = delta / 10000.0f;
		step      = delta / 1000.0f;
		largestep = delta / 40.0f;

		smallstep = std::max(1.0, rint(smallstep));
		step      = std::max(1.0, rint(step));
		largestep = std::max(1.0, rint(largestep));
	}
	/* else: leave all others as default '0'
	 * in that case the UI (eg. AutomationController::create)
	 * uses internal_to_interface() to map the value
	 * to an appropriate interface range
	 */
}

}  // namespace Evoral
