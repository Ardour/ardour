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

#pragma once

#include <string>
#include <vector>

#include "pbd/signals.h"

namespace ARDOUR {

enum MusicalModeType {
	AbsolutePitch,
	SemitoneSteps,
	WholeToneSteps,
	RatioSteps,
	RatioFromRoot,
	MidiNote,
};

class MusicalMode {
   public:

	enum Name {
		Dorian,
		IonianMajor,
		AeolianMinor,
		HarmonicMinor,
		MelodicMinorAscending,
		MelodicMinorDescending,
		Phrygian,
		Lydian,
		Mixolydian,
		Locrian,
		PentatonicMajor,
		PentatonicMinor,
		Chromatic,
		BluesScale,
		NeapolitanMinor,
		NeapolitanMajor,
		Oriental,
		DoubleHarmonic,
		Enigmatic,
		Hirajoshi,
		HungarianMinor,
		HungarianMajor,
		Kumoi,
		Iwato,
		Hindu,
		Spanish8Tone,
		Pelog,
		HungarianGypsy,
		Overtone,
		LeadingWholeTone,
		Arabian,
		Balinese,
		Gypsy,
		Mohammedan,
		Javanese,
		Persian,
		Algerian
	};

	MusicalMode (std::string const & name, MusicalModeType type, std::vector<float> const & elements);
	MusicalMode (MusicalMode const & other);
	MusicalMode (MusicalMode::Name);
	MusicalMode (std::ifstream& file); /* Read from a Scala file */

	MusicalMode operator= (MusicalMode const & other);

	std::string name() const { return _name; }
	MusicalModeType type() const { return _type; }
	int size() const { return _elements.size(); }
	std::vector<float> const & elements() const { return _elements; }

	std::vector<float> pitches_from_root (float root, int steps) const;
	std::vector<int> as_midi (int scale_root) const;
	void set_name (std::string const & str);

	PBD::Signal<void()> NameChanged;
	PBD::Signal<void()> Changed;

   protected:
	std::string _name;
	MusicalModeType _type;
	std::vector<float> _elements;

	void fill (Name);

	std::vector<float> absolute_pitch_pitches_from_root (float root, int steps) const;
	std::vector<float> semitone_steps_pitches_from_root (float root, int steps) const;
	std::vector<float> wholetone_steps_pitches_from_root (float root, int steps) const;
	std::vector<float> ratio_steps_pitches_from_root (float root, int steps) const;
	std::vector<float> ratio_from_root_pitches_from_root (float root, int steps) const;
	std::vector<float> midi_note_pitches_from_root (float root, int steps) const;

	std::vector<int> absolute_pitch_as_midi (int root) const;
	std::vector<int> semitone_steps_as_midi (int root) const;
	std::vector<int> wholetone_steps_as_midi (int root) const;
	std::vector<int> ratio_steps_as_midi (int root) const;
	std::vector<int> ratio_from_root_as_midi (int root) const;
	std::vector<int> midi_note_as_midi (int root) const;
};


class MusicalKey : public MusicalMode
{
    public:
	MusicalKey (float root, MusicalMode const &);
	MusicalKey (MusicalKey const & other);

	MusicalKey operator= (MusicalKey const & other);

	float root() const { return _root; }
	float nth (int n) const;

   private:
	float _root;

};

} // namespace
