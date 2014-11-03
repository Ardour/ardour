/*
    Copyright (C) 2014 Paul Davis
    Author: David Robillard

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_parameter_descriptor_h__
#define __ardour_parameter_descriptor_h__

#include "ardour/variant.h"
#include "evoral/Parameter.hpp"

namespace ARDOUR {

typedef std::map<const std::string, const float> ScalePoints;

/** Descriptor of a parameter or control.
 *
 * Essentially a union of LADSPA, VST and LV2 info.
 */
struct ParameterDescriptor
{
	enum Unit {
		NONE,       ///< No unit
		DB,         ///< Decibels
		MIDI_NOTE,  ///< MIDI note number
		HZ,         ///< Frequency in Hertz
	};

	ParameterDescriptor(const Evoral::Parameter& parameter)
		: key((uint32_t)-1)
		, datatype(Variant::VOID)
		, unit(NONE)
		, normal(parameter.normal())
		, lower(parameter.min())
		, upper(parameter.max())
		, step((upper - lower) / 100.0f)
		, smallstep((upper - lower) / 1000.0f)
		, largestep((upper - lower) / 10.0f)
		, integer_step(parameter.type() >= MidiCCAutomation &&
		               parameter.type() <= MidiChannelPressureAutomation)
		, toggled(parameter.toggled())
		, logarithmic(false)
		, sr_dependent(false)
		, min_unbound(0)
		, max_unbound(0)
		, enumeration(false)
	{
		if (parameter.type() == GainAutomation) {
			unit = DB;
		}
	}

	ParameterDescriptor()
		: key((uint32_t)-1)
		, datatype(Variant::VOID)
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

	/// Set step, smallstep, and largestep, based on current description
	void update_steps() {
		if (unit == ParameterDescriptor::MIDI_NOTE) {
			step      = smallstep = 1;  // semitone
			largestep = 12;             // octave
		} else {
			const float delta = upper - lower;

			step      = delta / 1000.0f;
			smallstep = delta / 10000.0f;
			largestep = delta / 10.0f;

			if (integer_step) {
				step      = rint(step);
				largestep = rint(largestep);
				// leave smallstep alone for fine tuning
			}
		}
	}

	std::string                    label;
	std::string                    print_fmt;  ///< format string for pretty printing
	boost::shared_ptr<ScalePoints> scale_points;
	uint32_t                       key;  ///< for properties
	Variant::Type                  datatype;  ///< for properties
	Unit                           unit;
	float                          normal;
	float                          lower;  ///< for frequencies, this is in Hz (not a fraction of the sample rate)
	float                          upper;  ///< for frequencies, this is in Hz (not a fraction of the sample rate)
	float                          step;
	float                          smallstep;
	float                          largestep;
	bool                           integer_step;
	bool                           toggled;
	bool                           logarithmic;
	bool                           sr_dependent;
	bool                           min_unbound;
	bool                           max_unbound;
	bool                           enumeration;
};

} // namespace ARDOUR

#endif // __ardour_parameter_descriptor_h__
