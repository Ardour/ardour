/*
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_value_as_string_h__
#define __ardour_value_as_string_h__

#include <stddef.h>

#include "ardour/dB.h"
#include "ardour/parameter_descriptor.h"

#include "pbd/i18n.h"

namespace ARDOUR {

inline std::string
value_as_string(const ARDOUR::ParameterDescriptor& desc,
                double                             v)
{
	char buf[32];

	if (desc.scale_points) {
		// Check if value is on a scale point
		for (ARDOUR::ScalePoints::const_iterator i = desc.scale_points->begin();
		     i != desc.scale_points->end();
		     ++i) {
			if (i->second == v) {
				return i->first;  // Found it, return scale point label
			}
		}
	}

	if (desc.toggled) {
		return v > 0 ? _("on") : _("off");
	}

	// Value is not a scale point, print it normally
	if (desc.unit == ARDOUR::ParameterDescriptor::MIDI_NOTE) {
		snprintf(buf, sizeof(buf), "%s", ParameterDescriptor::midi_note_name (rint(v)).c_str());
	} else if (desc.type == GainAutomation || desc.type == BusSendLevel || desc.type == TrimAutomation || desc.type == EnvelopeAutomation || desc.type == MainOutVolume) {
#ifdef PLATFORM_WINDOWS
		if (v < GAIN_COEFF_SMALL) {
			snprintf(buf, sizeof(buf), "-inf dB");
		} else {
			snprintf(buf, sizeof(buf), "%.2f dB", accurate_coefficient_to_dB (v));
		}
#else
		snprintf(buf, sizeof(buf), "%.2f dB", accurate_coefficient_to_dB (v));
#endif
	} else if (desc.type == PanWidthAutomation) {
		snprintf (buf, sizeof (buf), "%d%%", (int) floor (100.0 * v));
	} else if (!desc.print_fmt.empty()) {
		snprintf(buf, sizeof(buf), desc.print_fmt.c_str(), v);
	} else if (desc.integer_step) {
		snprintf(buf, sizeof(buf), "%d", (int)v);
	} else if (desc.upper - desc.lower >= 1000) {
		snprintf(buf, sizeof(buf), "%.1f", v);
	} else if (desc.upper - desc.lower >= 100) {
		snprintf(buf, sizeof(buf), "%.2f", v);
	} else {
		snprintf(buf, sizeof(buf), "%.3f", v);
	}
	if (desc.print_fmt.empty() && desc.unit == ARDOUR::ParameterDescriptor::DB) {
		// TODO: Move proper dB printing from AutomationLine here
		return std::string(buf) + " dB";
	}
	return buf;
}

inline std::string
value_as_string(const ARDOUR::ParameterDescriptor& desc,
                const ARDOUR::Variant&             val)
{
	// Only numeric support, for now
	return value_as_string(desc, val.to_double());
}

}  // namespace ARDOUR

#endif /* __ardour_value_as_string_h__ */
