/*
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <algorithm>
#include <boost/algorithm/string.hpp>

#include "pbd/control_math.h"

#include "ardour/amp.h"
#include "ardour/dB.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/parameter_types.h"
#include "ardour/rc_configuration.h"
#include "ardour/types.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

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
	, integer_step(parameter_is_midi (parameter.type ()))
	, sr_dependent(false)
	, enumeration(false)
	, inline_ctrl(false)
	, display_priority(0)
{
	ScalePoints sp;

	/* Note: defaults in Evoral::ParameterDescriptor */

	switch((AutomationType)parameter.type()) {
	case BusSendLevel:
		inline_ctrl = true;
		/* fallthrough */
	case GainAutomation:
		upper  = Config->get_max_gain();
		normal = 1.0f;
		break;
	case BusSendEnable:
		upper  = 1.f;
		normal = 1.f;
		toggled = true;
		break;
	case TrimAutomation:
		upper  = 10; // +20dB
		lower  = .1; // -20dB
		normal = 1.0f;
		logarithmic = true;
		break;
	case MainOutVolume:
		upper  = 100; // +40dB
		lower  = .01; // -40dB
		normal = 1.0f;
		logarithmic = true;
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
	case RecSafeAutomation:
		lower  = 0.0;
		upper  = 1.0;
		toggled = true;
		break;
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
	case MidiNotePressureAutomation:
		lower  = 0.0;
		normal = 0.0;
		upper  = 127.0;
		print_fmt = "%.0f";
		break;
	case MidiPitchBenderAutomation:
		lower  = 0.0;
		normal = 8192.0;
		upper  = 16383.0;
		print_fmt = "%.0f";
		break;
	case PhaseAutomation:
		toggled = true;
		scale_points = boost::shared_ptr<ScalePoints>(new ScalePoints());
		scale_points->insert (std::make_pair (_("Normal"), 0));
		scale_points->insert (std::make_pair (_("Invert"), 1));
		break;
	case MonitoringAutomation:
		enumeration = true;
		integer_step = true;
		lower = MonitorAuto;
		upper = MonitorCue;
		scale_points = boost::shared_ptr<ScalePoints>(new ScalePoints());
		scale_points->insert (std::make_pair (_("Auto"), MonitorAuto));
		scale_points->insert (std::make_pair (_("Input"), MonitorInput));
		scale_points->insert (std::make_pair (_("Disk"), MonitorDisk));
		break;
	case SoloIsolateAutomation:
	case SoloSafeAutomation:
		toggled = true;
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
	, type(NullAutomation)
	, unit(NONE)
	, step(0)
	, smallstep(0)
	, largestep(0)
	, integer_step(false)
	, sr_dependent(false)
	, enumeration(false)
	, inline_ctrl(false)
	, display_priority(0)
{}

void
ParameterDescriptor::update_steps()
{
	/* sanitize flags */
	if (toggled || enumeration) {
		logarithmic = false;
	}
	if (logarithmic && sr_dependent && upper > lower && lower == 0) {
		/* work-around for plugins with a log-scale control 0..SR; log (0) is not defined */
		lower = upper / 1000.f;
	}
	if (logarithmic && (upper <= lower || lower * upper <= 0)) {
		/* log-scale params need upper > lower and both values need the same sign */
		logarithmic = false;
	}
	if (rangesteps < 2) {
		rangesteps = 0;
	}
	if (enumeration) {
		/* enums need scale-points.
		 * The GUI is more restrictive, a dropdown is displayed
		 * IIF  scale_points.size() == (1 + upper - lower)
		 */
		if (!scale_points || scale_points->empty ()) {
			enumeration = false;
		}
	}
	if (integer_step) {
		if (lower >= upper) {
			integer_step = false;
		}
	}

	/* upper == lower does not make any sense */
	if (lower == upper) {
		upper = lower + 0.01; // add some arbitrary value
	}

	/* set steps */

	if (unit == ParameterDescriptor::MIDI_NOTE) {
		step      = smallstep = 1;  // semitone
		largestep = 12;             // octave
	} else if (type == GainAutomation || type == TrimAutomation || type == BusSendLevel || type == MainOutVolume) {
		/* dB_coeff_step gives a step normalized for [0, max_gain].  This is
		   like "slider position", so we convert from "slider position" to gain
		   to have the correct unit here. */
		largestep = position_to_gain (dB_coeff_step(upper));
		step      = position_to_gain (largestep / 10.0);
		smallstep = step;
	} else if (logarithmic) {
		/* ignore logscale rangesteps. {small|large}steps are used with the spinbox.
		 * gtk-spinbox shows the internal (not interface) value and up/down
		 * arrows linearly increase.
		 * The AutomationController uses internal_to_interface():
		 *   ui-step [0..1] -> log (1 + largestep / lower) / log (upper / lower)
		 * so we use a step that's a multiple of "lower" for the interface step:
		 *   log (1 + x) / log (upper / lower)
		 */
		smallstep = step = lower / 11;
		largestep = lower / 3;
		/* NOTE: the actual value does use rangesteps via
		 * logscale_to_position_with_steps(), position_to_logscale_with_steps()
		 * when it is converted.
		 */
	} else if (rangesteps > 1) {
		const float delta = upper - lower;
		if (integer_step) {
			smallstep = step = 1.0;
			largestep = std::max(1.f, rintf (delta / (rangesteps - 1.f)));
		} else {
			step = smallstep = delta / (rangesteps - 1.f);
			largestep = std::min ((delta / 4.0f), 10.f * smallstep);
		}
	} else {
		const float delta = upper - lower;
		/* 30 steps between min/max (300 for fine-grained) */
		if (integer_step) {
			smallstep = step = 1.0;
			largestep = std::max(1.f, rintf (delta / 30.f));
		} else {
			step      = smallstep = (delta / 300.0f);
			largestep = (delta / 30.0f);
		}
	}
}

std::string
ParameterDescriptor::midi_note_name (const uint8_t b, bool translate)
{
	char buf[16];
	if (b > 127) {
		snprintf(buf, sizeof(buf), "%d", b);
		return buf;
	}

	static const char* en_notes[] = {
		"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
	};

	static const char* notes[] = {
		S_("Note|C"),
		S_("Note|C#"),
		S_("Note|D"),
		S_("Note|D#"),
		S_("Note|E"),
		S_("Note|F"),
		S_("Note|F#"),
		S_("Note|G"),
		S_("Note|G#"),
		S_("Note|A"),
		S_("Note|A#"),
		S_("Note|B")
	};

	/* MIDI note 0 is in octave -1 (in scientific pitch notation) */
	const int octave = b / 12 - 1;
	const size_t p = b % 12;
	snprintf (buf, sizeof (buf), "%s%d", translate ? notes[p] : en_notes[p], octave);
	return buf;
}

std::string
ParameterDescriptor::normalize_note_name(const std::string& name)
{
	// Remove whitespaces and convert to lower case for a more resilient parser
	return boost::to_lower_copy(boost::erase_all_copy(name, " "));
};

ParameterDescriptor::NameNumMap
ParameterDescriptor::build_midi_name2num()
{
	NameNumMap name2num;
	for (uint8_t num = 0; num < 128; num++) {
		name2num[normalize_note_name(midi_note_name(num))] = num;
	}
	return name2num;
}

uint8_t
ParameterDescriptor::midi_note_num (const std::string& name)
{
	static NameNumMap name2num = build_midi_name2num();

	uint8_t num = -1; // -1 (or 255) is returned in case of failure

	NameNumMap::const_iterator it = name2num.find(normalize_note_name(name));
	if (it != name2num.end())
		num = it->second;

	return num;
}

float
ParameterDescriptor::to_interface (float val, bool rotary) const
{
	val = std::min (upper, std::max (lower, val));
	switch(type) {
		case GainAutomation:
			/* fallthrough */
		case BusSendLevel:
			/* fallthrough */
		case EnvelopeAutomation:
			val = gain_to_slider_position_with_max (val, upper);
			break;
		case TrimAutomation:
			/* fallthrough */
		case MainOutVolume:
			{
				const float lower_db = accurate_coefficient_to_dB (lower);
				const float range_db = accurate_coefficient_to_dB (upper) - lower_db;
				val = (accurate_coefficient_to_dB (val) - lower_db) / range_db;
			}
			break;
		case PanAzimuthAutomation:
			if (!rotary) {
				val = 1.0 - val;
			}
			break;
		case PanElevationAutomation:
			// val = val;
			break;
		case PanWidthAutomation:
			val = .5f + val * .5f;
			break;
		default:
			if (logarithmic) {
				if (rangesteps > 1) {
					val = logscale_to_position_with_steps (val, lower, upper, rangesteps);
				} else {
					val = logscale_to_position (val, lower, upper);
				}
			} else if (toggled) {
				return (val - lower) / (upper - lower) >= 0.5f ? 1.f : 0.f;
			} else if (integer_step) {
				/* evenly-divide steps. lower,upper inclusive
				 * e.g. 5 integers 0,1,2,3,4 are mapped to a fader
				 * [0.0 ... 0.2 | 0.2 ... 0.4 | 0.4 ... 0.6 | 0.6 ... 0.8 | 0.8 ... 1.0]
				 *       0             1             2             3             4
				 *      0.1           0.3           0.5           0.7           0.9
				 */
				val = (val + .5f - lower) / (1.f + upper - lower);
			} else {
				val = (val - lower) / (upper - lower);
			}
			break;
	}
	val = std::max (0.f, std::min (1.f, val));
	return val;
}

float
ParameterDescriptor::from_interface (float val, bool rotary) const
{
	val = std::max (0.f, std::min (1.f, val));

	switch(type) {
		case GainAutomation:
		case EnvelopeAutomation:
		case BusSendLevel:
			val = slider_position_to_gain_with_max (val, upper);
			break;
		case TrimAutomation:
			{
				const float lower_db = accurate_coefficient_to_dB (lower);
				const float range_db = accurate_coefficient_to_dB (upper) - lower_db;
				val = dB_to_coefficient (lower_db + val * range_db);
			}
			break;
		case PanAzimuthAutomation:
			if (!rotary) {
				val = 1.0 - val;
			}
			break;
		case PanElevationAutomation:
			 // val = val;
			break;
		case PanWidthAutomation:
			val = 2.f * val - 1.f;
			break;
		default:
			if (logarithmic) {
				assert (!toggled && !integer_step); // update_steps() should prevent that.
				if (rangesteps > 1) {
					val = position_to_logscale_with_steps (val, lower, upper, rangesteps);
				} else {
					val = position_to_logscale (val, lower, upper);
				}
			} else if (toggled) {
				val = val >= 0.5 ? upper : lower;
			} else if (integer_step) {
				/* upper and lower are inclusive. use evenly-divided steps
				 * e.g. 5 integers 0,1,2,3,4 are mapped to a fader
				 * [0.0 .. 0.2 | 0.2 .. 0.4 | 0.4 .. 0.6 | 0.6 .. 0.8 | 0.8 .. 1.0]
				 */
				val = floor (lower + val * (1.f + upper - lower));
			} else if (rangesteps > 1) {
				/* similar to above, but for float controls */
				val = round (val * (rangesteps - 1.f)) / (rangesteps - 1.f); // XXX
				val = val * (upper - lower) + lower;
			} else {
				val = val * (upper - lower) + lower;
			}
			break;
	}
	val = std::min (upper, std::max (lower, val));
	return val;
}

bool
ParameterDescriptor::is_linear () const
{
	if (logarithmic) {
		return false;
	}
	switch(type) {
		case GainAutomation:
		case EnvelopeAutomation:
		case BusSendLevel:
			return false;
		default:
			break;
	}
	return true;
}

float
ParameterDescriptor::compute_delta (float from, float to) const
{
	if (is_linear ()) {
		return to - from;
	}
	if (from == 0) {
		return 0;
	}
	return to / from;
}

float
ParameterDescriptor::apply_delta (float val, float delta) const
{
	if (is_linear ()) {
		return val + delta;
	} else {
		return val * delta;
	}
}

float
ParameterDescriptor::step_enum (float val, bool prev) const
{
	if (!enumeration) {
		return val;
	}
	assert (scale_points && !scale_points->empty ());
	float rv = scale_points->begin()->second;
	float delta = fabsf (val - rv);
	std::vector<float> avail;

	for (ScalePoints::const_iterator i = scale_points->begin (); i != scale_points->end (); ++i) {
		float s = i->second;
		avail.push_back (s);
		if (fabsf (val - s) < delta) {
			rv = s;
			delta = fabsf (val - s);
		}
	}
	/* ScalePoints map is sorted by text string */
	std::sort (avail.begin (), avail.end ());
	std::vector<float>::const_iterator it = std::find (avail.begin (), avail.end (), rv);
	assert (it != avail.end());

	if (prev) {
		if (it == avail.begin()) {
			return rv;
		}
		return *(--it);
	} else {
		if (++it == avail.end()) {
			return rv;
		}
		return *(it);
	}
}

} // namespace ARDOUR
