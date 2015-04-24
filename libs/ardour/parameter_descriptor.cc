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

#include "ardour/amp.h"
#include "ardour/dB.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/rc_configuration.h"
#include "ardour/types.h"
#include "ardour/utils.h"

namespace ARDOUR {

ParameterDescriptor::ParameterDescriptor(const Evoral::Parameter& parameter)
	: Evoral::ParameterDescriptor()
	, key((uint32_t)-1)
	, datatype(Variant::NOTHING)
	, type((AutomationType)parameter.type())
	, unit(NONE)
	, step(0)
	, smallstep(0)
	, largestep(0)
	, integer_step(parameter.type() >= MidiCCAutomation &&
	               parameter.type() <= MidiChannelPressureAutomation)
	, logarithmic(false)
	, sr_dependent(false)
	, min_unbound(0)
	, max_unbound(0)
	, enumeration(false)
{
	switch((AutomationType)parameter.type()) {
	case GainAutomation:
		upper  = Config->get_max_gain();
		normal = 1.0f;
		break;
	case TrimAutomation:
		upper  = 10; // +20dB
		lower  = .1; // -20dB
		normal = 1.0f;
		break;
	case PanAzimuthAutomation:
		normal = 0.5f; // there really is no _normal but this works for stereo, sort of
		upper  = 1.0f;
		break;
	case PanWidthAutomation:
		lower  = -1.0;
		upper  = 1.0;
		normal = 0.0f;
		break;
	case RecEnableAutomation:
		lower  = 0.0;
		upper  = 1.0;
		toggled = true;
		break;
	case PluginAutomation:
	case FadeInAutomation:
	case FadeOutAutomation:
	case EnvelopeAutomation:
		upper  = 2.0f;
		normal = 1.0f;
		break;
	case SoloAutomation:
	case MuteAutomation:
		upper  = 1.0f;
		normal = 0.0f;
		toggled = true;
		break;
	case MidiCCAutomation:
	case MidiPgmChangeAutomation:
	case MidiChannelPressureAutomation:
		lower  = 0.0;
		normal = 0.0;
		upper  = 127.0;
		break;
	case MidiPitchBenderAutomation:
		lower  = 0.0;
		normal = 8192.0;
		upper  = 16383.0;
		break;
	default:
		break;
	}

	update_steps();
}

ParameterDescriptor::ParameterDescriptor()
	: Evoral::ParameterDescriptor()
	, key((uint32_t)-1)
	, datatype(Variant::NOTHING)
	, unit(NONE)
	, step(0)
	, smallstep(0)
	, largestep(0)
	, integer_step(false)
	, logarithmic(false)
	, sr_dependent(false)
	, min_unbound(0)
	, max_unbound(0)
	, enumeration(false)
{}

void
ParameterDescriptor::update_steps()
{
	if (unit == ParameterDescriptor::MIDI_NOTE) {
		step      = smallstep = 1;  // semitone
		largestep = 12;             // octave
	} else if (type == GainAutomation || type == TrimAutomation) {
		/* dB_coeff_step gives a step normalized for [0, max_gain].  This is
		   like "slider position", so we convert from "slider position" to gain
		   to have the correct unit here. */
		largestep = slider_position_to_gain(dB_coeff_step(upper));
		step      = slider_position_to_gain(largestep / 10.0);
		smallstep = step;
	} else {
		const float delta = upper - lower;

		/* 30 happens to be the total number of steps for a fader with default
		   max gain of 2.0 (6 dB), so we use 30 here too for consistency. */
		step      = smallstep = (delta / 300.0f);
		largestep = (delta / 30.0f);

		if (logarithmic) {
			/* Steps are linear, but we map them with pow like values (in
			   internal_to_interface).  Thus, they are applied exponentially,
			   which means too few steps.  So, divide to get roughly the
			   desired number of steps (30).  This is not mathematically
			   precise but seems to be about right for the controls I tried.
			   If you're reading this, you've probably found a case where that
			   isn't true, and somebody needs to sit down with a piece of paper
			   and actually do the math. */
			smallstep = smallstep / logf(30.0f);
			step      = step      / logf(30.0f);
			largestep = largestep / logf(30.0f);
		} else if (integer_step) {
			smallstep = std::max(1.0, rint(smallstep));
			step      = std::max(1.0, rint(step));
			largestep = std::max(1.0, rint(largestep));
		}
	}
}

} // namespace ARDOUR
