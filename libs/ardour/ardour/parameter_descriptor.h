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

#include "ardour/types.h"
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

	static std::string midi_note_name (uint8_t, bool translate=true);

	/** Dual of midi_note_name, convert a note name into its midi note number. */
	typedef std::map<std::string, uint8_t> NameNumMap;
	static std::string normalize_note_name(const std::string& name);
	static NameNumMap build_midi_name2num();
	static uint8_t midi_note_num (const std::string& name);

	ParameterDescriptor(const Evoral::Parameter& parameter);

	ParameterDescriptor();

	/** control-value to normalized [0..1] range
	 *
	 * Convert given AutomationType from lower/upper range to [0..1]
	 * interface value, using settings from Evoral::ParameterDescriptor.
	 *
	 * default for AutomationControl::internal_to_interface ();
	 */
	float to_interface (float) const;

	/** normalized [0..1] to control-value range
	 *
	 * Convert [0..1] to the control's range of this AutomationType
	 * using settings from Evoral::ParameterDescriptor.
	 *
	 * default for AutomationControl::interface_to_internal ();
	 */
	float from_interface (float) const;

	bool  is_linear () const;
	float compute_delta (float from, float to) const;
	float apply_delta (float value, float delta) const;

	/* find the closest scale-point, return the internal value of
	 * the prev/next scale-point (no wrap-around)
	 *
	 * If the given parameter is not en enum, the given val is returned.
	 *
	 * @param val internal (not interface) value
	 * @param prev if true, step to prev scale-point, otherwise next
	 * @return internal value, suitable for set_value()
	 */
	float step_enum (float val, bool prev) const;

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
	bool                           sr_dependent;
	bool                           enumeration;
};

} // namespace ARDOUR

#endif // __ardour_parameter_descriptor_h__
