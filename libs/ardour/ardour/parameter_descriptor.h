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
#include "evoral/ParameterDescriptor.hpp"

namespace ARDOUR {

typedef std::map<const std::string, const float> ScalePoints;

/** Descriptor of a parameter or control.
 *
 * Essentially a union of LADSPA, VST and LV2 info.
 */
struct LIBARDOUR_API ParameterDescriptor : public Evoral::ParameterDescriptor
{
	enum Unit {
		NONE,       ///< No unit
		DB,         ///< Decibels
		MIDI_NOTE,  ///< MIDI note number
		HZ,         ///< Frequency in Hertz
	};

	ParameterDescriptor(const Evoral::Parameter& parameter);

	ParameterDescriptor();

	/** Set step, smallstep, and largestep, based on current description. */
	void update_steps();

	std::string                    label;
	std::string                    print_fmt;  ///< format string for pretty printing
	boost::shared_ptr<ScalePoints> scale_points;
	uint32_t                       key;  ///< for properties
	Variant::Type                  datatype;  ///< for properties
	AutomationType                 type;
	Unit                           unit;
	float                          step;
	float                          smallstep;
	float                          largestep;
	bool                           integer_step;
	bool                           logarithmic;
	bool                           sr_dependent;
	bool                           min_unbound;
	bool                           max_unbound;
	bool                           enumeration;
};

} // namespace ARDOUR

#endif // __ardour_parameter_descriptor_h__
