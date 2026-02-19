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

#pragma once

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
		for (auto const & [label,val] : *desc.scale_points) {
			if (val == v) {
				return label;
			}
		}
	}

	if (desc.toggled) {
		return v > 0 ? _("on") : _("off");
	}

	// Value is not a scale point, print it normally
	if (desc.unit == ARDOUR::ParameterDescriptor::MIDI_NOTE) {
		snprintf(buf, sizeof(buf), "%s", ParameterDescriptor::midi_note_name (rint(v)).c_str());
	} else if (desc.type == GainAutomation
	           || desc.type == BusSendLevel
	           || desc.type == TrimAutomation
	           || desc.type == EnvelopeAutomation
	           || desc.type == MainOutVolume
	           || desc.type == SurroundSendLevel
	           || desc.type == InsertReturnLevel) {
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

inline double
string_as_value (const ARDOUR::ParameterDescriptor& desc,
                 std::string const & str,
                 bool& legal)
{
	legal = true; /* be optimistic */

	if (desc.scale_points) {
		// Check if label matches a scale point
		for (auto const & [label,value] : *desc.scale_points) {
			if (label == str) {
				return value;  // Found it, return scale point value
			}
		}
		legal = false;
		return 0.;
	}

	if (desc.toggled) {
		if (str == _("on") || str == _("yes") || str == "1") {
			return 1.0;
		} else if (str == _("off") || str == _("no") || str == "0") {
			return 0.0;
		} else {
			legal = false;
			return 0.;
		}
	}

	// Value is not a scale point, print it normally
	if (desc.unit == ARDOUR::ParameterDescriptor::MIDI_NOTE) {

		uint8_t nn = ARDOUR::ParameterDescriptor::midi_note_num (str);
		legal = (nn == 255);
		return nn;

	} else if (desc.type == GainAutomation ||
	           desc.type == TrimAutomation ||
	           desc.type == BusSendLevel ||
	           desc.type == EnvelopeAutomation ||
	           desc.type == MainOutVolume ||
	           desc.type == SurroundSendLevel ||
	           desc.type == InsertReturnLevel ||
	           desc.unit == ARDOUR::ParameterDescriptor::DB) {

		float f;
		legal = (sscanf (str.c_str(), "%f", &f) == 1);
		if (!legal) {
			return 0.;
		}

		/* clamp to range */

		float max_dB = accurate_coefficient_to_dB (desc.upper);
		float min_dB = accurate_coefficient_to_dB (desc.lower);

		f = std::max (std::min (f, max_dB), min_dB);

		return dB_to_coefficient(f);

	} else if (desc.type == PanWidthAutomation) {
		int tmp;
		legal = (sscanf (str.c_str(), "%d", &tmp) == 1);
		return tmp;
	} else if (desc.integer_step) {
		float tmp;
		legal = (sscanf (str.c_str(), "%g", &tmp) == 1);
		return (int) tmp;
	}

	legal = false;
	return 0.;
}

}  // namespace ARDOUR
