/*
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_parameter_descriptor_h__
#define __ardour_parameter_descriptor_h__

#include "pbd/natsort.h"
#include "ardour/types.h"
#include "ardour/variant.h"

#include "evoral/Parameter.h"
#include "evoral/ParameterDescriptor.h"

namespace ARDOUR {

struct CompareNumericallyLess {
	bool operator() (std::string const& a, std::string const& b) const {
		return PBD::numerically_less (a.c_str(), b.c_str());
	}
};

typedef std::map<const std::string, const float, CompareNumericallyLess> ScalePoints;

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
	 *
	 * @param v the control-value to convert
	 * @param rotary set to true if the GUI control is a rotary knob
	 * @return interface value in range 0..1
	 */
	float to_interface (float v, bool rotary = false) const;

	/** normalized [0..1] to control-value range
	 *
	 * Convert [0..1] to the control's range of this AutomationType
	 * using settings from Evoral::ParameterDescriptor.
	 *
	 * default for AutomationControl::interface_to_internal ();
	 *
	 * @param v the value in range 0..1 to on convert
	 * @param rotary set to true if the GUI control is a rotary knob
	 * @return control-value in range lower..upper
	 */
	float from_interface (float v, bool rotary = false) const;

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
	bool                           inline_ctrl;
	uint32_t                       display_priority; ///< higher is more important http://lv2plug.in/ns/ext/port-props#displayPriority
};

} // namespace ARDOUR

#endif // __ardour_parameter_descriptor_h__
