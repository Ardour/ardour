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

#ifndef EVORAL_MIDI_PARAMETERS_HPP
#define EVORAL_MIDI_PARAMETERS_HPP

#include "evoral/visibility.h"

namespace Evoral {
namespace MIDI {

struct LIBEVORAL_API ContinuousController : public Parameter {
	ContinuousController(uint32_t cc_type, uint8_t channel, uint32_t controller)
		: Parameter(cc_type, channel, controller) {}
};

struct LIBEVORAL_API ProgramChange : public Parameter {
	ProgramChange(uint32_t pc_type, uint8_t channel) : Parameter(pc_type, channel, 0) {}
};

struct LIBEVORAL_API ChannelPressure : public Parameter {
	ChannelPressure(uint32_t ca_type, uint32_t channel) : Parameter(ca_type, channel, 0) {}
};

struct LIBEVORAL_API PitchBender : public Parameter {
	PitchBender(uint32_t pb_type, uint32_t channel) : Parameter(pb_type, channel, 0) {}
};

inline static void controller_range(double& min, double& max, double& normal) {
	min = 0.0;
	normal = 0.0;
	max = 127.0;
}

inline static void bender_range(double& min, double& max, double& normal) {
	min = 0.0;
	normal = 8192.0;
	max = 16383.0;
}

} // namespace MIDI
} // namespace Evoral

#endif // EVORAL_MIDI_PARAMETERS_HPP
