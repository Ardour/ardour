/*
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
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

#ifndef EVORAL_PARAMETER_DESCRIPTOR_HPP
#define EVORAL_PARAMETER_DESCRIPTOR_HPP

namespace Evoral {

/** Description of the value range of a parameter or control. */
struct ParameterDescriptor
{
	ParameterDescriptor()
		: normal(0.0)
		, lower(0.0)
		, upper(1.0)
		, toggled(false)
		, logarithmic(false)
		, rangesteps (0)
	{}

	float normal;      ///< Default value
	float lower;       ///< Minimum value (in Hz, for frequencies)
	float upper;       ///< Maximum value (in Hz, for frequencies)
	bool  toggled;     ///< True iff parameter is boolean
	bool  logarithmic; ///< True for log-scale parameters
	unsigned int rangesteps; ///< number of steps, [min,max] (inclusive). <= 1 means continuous. == 2 only min, max. For integer controls this is usually (1 + max - min)
};

} // namespace Evoral

#endif // EVORAL_PARAMETER_DESCRIPTOR_HPP
