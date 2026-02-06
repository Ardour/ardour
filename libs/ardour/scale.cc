/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cmath>

#include "pbd/enumwriter.h"

#include "ardour/scale.h"

using namespace ARDOUR;

MusicalMode::MusicalMode (std::string const & name, MusicalModeType type, std::vector<float> const & elements)
	: _name (name)
	, _type (type)
	, _elements (elements)
{
}

MusicalMode::MusicalMode (MusicalMode const & other)
	: _name (other._name)
	, _type (other._type)
	, _elements (other._elements)
{
}

void
MusicalMode::set_name (std::string const & str)
{
	_name = str;
	NameChanged(); /* EMIT SIGNAL */
}

std::vector<float>
MusicalMode::pitches_from_root (float root, int steps) const
{
	switch (_type) {
	case AbsolutePitch:
		return absolute_pitch_pitches_from_root (root, steps);
	case SemitoneSteps:
		return semitone_steps_pitches_from_root (root, steps);
	case WholeToneSteps:
		return wholetone_steps_pitches_from_root (root, steps);
	case RatioSteps:
		return ratio_steps_pitches_from_root (root, steps);
	case RatioFromRoot:
		return ratio_from_root_pitches_from_root (root, steps);
	}

	/*NOTREACHED*/
	return std::vector<float> ();
}

std::vector<float>
MusicalMode::absolute_pitch_pitches_from_root (float root, int steps) const
{
	std::vector<float> pitches;
	return pitches;
}

std::vector<float>
MusicalMode::semitone_steps_pitches_from_root (float root, int steps) const
{
	std::vector<float> pitches;
	return pitches;
}

std::vector<float>
MusicalMode::wholetone_steps_pitches_from_root (float root, int steps) const
{
	std::vector<float> pitches;
	return pitches;
}

std::vector<float>
MusicalMode::ratio_steps_pitches_from_root (float root, int steps) const
{
	std::vector<float> pitches;
	return pitches;
}

std::vector<float>
MusicalMode::ratio_from_root_pitches_from_root (float root, int steps) const
{
	std::vector<float> pitches;
	return pitches;
}

void
MusicalMode::fill (Name nom)
{
	_elements.clear ();
	_type = WholeToneSteps;
	_name = enum_2_string (nom);

	/* scales/modes as distances from root, expressed
	   in fractional whole tones.
	*/

	switch (nom) {
	case Dorian:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (2.0);
		_elements.push_back (3.0);
		_elements.push_back (4.0);
		_elements.push_back (4.5);
		_elements.push_back (5.5);
		break;
	case IonianMajor:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.5);
		_elements.push_back (5.5);
		break;
	case AeolianMinor:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		break;
	case HarmonicMinor:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (5.0);
		_elements.push_back (5.5);
		break;
	case  BluesScale:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (3);
		_elements.push_back (3.5);
		_elements.push_back (4.5);
		_elements.push_back (5.0);
		_elements.push_back (5.5);
		break;
	case MelodicMinorAscending:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.5);
		_elements.push_back (5.5);
		break;
	case MelodicMinorDescending:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.5);
		_elements.push_back (5.0);
		break;
	case Phrygian:
		_elements.push_back (0.5);
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		break;
	case Lydian:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (3.0);
		_elements.push_back (3.5);
		_elements.push_back (4.5);
		_elements.push_back (5.5);
		break;
	case Mixolydian:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.5);
		_elements.push_back (5.0);
		break;
	case Locrian:
		_elements.push_back (0.5);
		_elements.push_back (1.5);
		_elements.push_back (2.0);
		_elements.push_back (3.0);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		break;
	case PentatonicMajor:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		break;
	case PentatonicMinor:
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (5.0);
		break;
	case  Chromatic:
		_elements.push_back (0.5);
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.0);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (4.5);
		_elements.push_back (5.0);
		_elements.push_back (5.5);
		break;
	case  NeapolitanMinor:
		_elements.push_back (0.5);
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (2.5);
		_elements.push_back (4.0);
		_elements.push_back (5.5);
		break;
	case  NeapolitanMajor:
		_elements.push_back (0.5);
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.5);
		_elements.push_back (5.5);
		break;
	case  Oriental:
		_elements.push_back (0.5);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.0);
		_elements.push_back (4.5);
		_elements.push_back (5.0);
		break;
	case  DoubleHarmonic:
		_elements.push_back (0.5);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.5);
		break;
	case  Enigmatic:
		_elements.push_back (0.5);
		_elements.push_back (2.0);
		_elements.push_back (3.0);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		_elements.push_back (5.5);
		break;
	case  Hirajoshi:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		break;
	case  HungarianMinor:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (3.0);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.5);
		break;
	case  HungarianMajor:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (3.0);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		break;
	case  Kumoi:
		_elements.push_back (0.5);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		break;
	case  Iwato:
		_elements.push_back (0.5);
		_elements.push_back (2.5);
		_elements.push_back (3.0);
		_elements.push_back (5.0);
		break;
	case  Hindu:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		break;
	case  Spanish8Tone:
		_elements.push_back (0.5);
		_elements.push_back (1.5);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.0);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		break;
	case  Pelog:
		_elements.push_back (0.5);
		_elements.push_back (1.5);
		_elements.push_back (3.5);
		_elements.push_back (5.0);
		break;
	case  HungarianGypsy:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (3.0);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		break;
	case  Overtone:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (3.0);
		_elements.push_back (3.5);
		_elements.push_back (4.5);
		_elements.push_back (5.0);
		break;
	case  LeadingWholeTone:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (3.0);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		_elements.push_back (5.5);
		break;
	case  Arabian:
		_elements.push_back (1.0);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.0);
		_elements.push_back (4.0);
		_elements.push_back (5.0);
		break;
	case  Balinese:
		_elements.push_back (0.5);
		_elements.push_back (1.5);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		break;
	case  Gypsy:
		_elements.push_back (0.5);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.5);
		break;
	case  Mohammedan:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.5);
		break;
	case  Javanese:
		_elements.push_back (0.5);
		_elements.push_back (1.5);
		_elements.push_back (2.5);
		_elements.push_back (3.5);
		_elements.push_back (4.5);
		_elements.push_back (5.0);
		break;
	case  Persian:
		_elements.push_back (0.5);
		_elements.push_back (2.0);
		_elements.push_back (2.5);
		_elements.push_back (3.0);
		_elements.push_back (4.0);
		_elements.push_back (5.5);
		break;
	case  Algerian:
		_elements.push_back (1.0);
		_elements.push_back (1.5);
		_elements.push_back (3.0);
		_elements.push_back (3.5);
		_elements.push_back (4.0);
		_elements.push_back (5.5);
		_elements.push_back (6.0);
		_elements.push_back (7.0);
		_elements.push_back (7.5);
		_elements.push_back (8.5);
		break;
	}
}

/** Return a sorted vector of all notes in a musical mode.
 *
 * The returned vector has every possible MIDI note number (0 through 127
 * inclusive) that is in the mode in any octave.
 */
std::vector<int>
MusicalMode::as_midi (int scale_root) const
{
	switch (_type) {
	case AbsolutePitch:
		return absolute_pitch_as_midi (scale_root);
	case SemitoneSteps:
		return semitone_steps_as_midi (scale_root);
	case WholeToneSteps:
		return wholetone_steps_as_midi (scale_root);
	case RatioSteps:
		return ratio_steps_as_midi (scale_root);
	case RatioFromRoot:
		return ratio_from_root_as_midi (scale_root);
	}

	/*NOTREACHED*/
	return std::vector<int> ();
}

std::vector<int>
MusicalMode::absolute_pitch_as_midi (int root) const
{
	std::vector<int> midi_notes;
	return midi_notes;
}

std::vector<int>
MusicalMode::semitone_steps_as_midi (int root) const
{
	std::vector<int> midi_notes;
	return midi_notes;
}

std::vector<int>
MusicalMode::wholetone_steps_as_midi (int scale_root) const
{
	std::vector<int> midi_notes;

	int root = scale_root - 12;

	// Repeatedly loop through the intervals in an octave
	for (std::vector<float>::const_iterator i = _elements.begin ();;) {
		if (i == _elements.end ()) {
			// Reached the end of the scale, continue with the next octave
			root += 12;
			if (root > 127) {
				break;
			}

			midi_notes.push_back (root);
			i = _elements.begin ();

		} else {
			const int note = (int)floor (root + (2.0 * (*i)));
			if (note > 127) {
				break;
			}

			if (note > 0) {
				midi_notes.push_back (note);
			}

			++i;
		}
	}

	return midi_notes;
}

std::vector<int>
MusicalMode::ratio_steps_as_midi (int root) const
{
	std::vector<int> midi_notes;
	return midi_notes;
}

std::vector<int>
MusicalMode::ratio_from_root_as_midi (int root) const
{
	std::vector<int> midi_notes;
	return midi_notes;
}


/*---------*/

MusicalKey::MusicalKey (float root, MusicalMode const & sc)
	: MusicalMode (sc)
	, _root (root)
{
}

float
MusicalKey::nth (int n) const
{
	if (n >= _elements.size()) {
		return -1;
	}

#warning paul you need to fix this
	return 99;
}

