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

namespace ARDOUR {

typedef std::map<const std::string, const float> ScalePoints;

/** Descriptor of a parameter or control.
 *
 * Essentially a union of LADSPA, VST and LV2 info.
 */
struct ParameterDescriptor
{
	ParameterDescriptor()
		: key((uint32_t)-1)
		, datatype(Variant::VOID)
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
		, midinote(false)
	{}

	std::string                    label;
	boost::shared_ptr<ScalePoints> scale_points;
	uint32_t                       key;  ///< for properties
	Variant::Type                  datatype;  ///< for properties
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
	bool                           midinote;  ///< only used if integer_step is also true
};

} // namespace ARDOUR

#endif // __ardour_parameter_descriptor_h__
