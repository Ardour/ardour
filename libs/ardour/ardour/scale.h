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

#include <map>
#include <string>
#include <vector>

#include "pbd/signals.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API MusicalMode
{
  public:
	enum Name {
		IonianMajor,
		AeolianMinor,
		Dorian,
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

	/* In theory we should be able to overload the singuler name, but at
	   least gcc is unable to resolve whether a single std::string as the
	   second argument is meant to be an initializer list or not. So we
	   provide two versions, one plural one singular.

	   The singular version should be used where there is only one accepted
	   name for a scale; the plural (initializer list) version should be
	   used where there is more than one.
	*/
	static void register_scale (TuningSystem, std::string const &, MusicalModeType, std::vector<float> const &);
	static void register_scales (TuningSystem, std::vector<std::string> const &, MusicalModeType, std::vector<float> const &);

	MusicalMode (TuningSystem, std::string const & name, MusicalModeType type, std::vector<float> const & elements);
	MusicalMode (MusicalMode const & other);
	MusicalMode (std::string const & name);
	MusicalMode (std::ifstream& file); /* Read from a Scala file */
	virtual ~MusicalMode() {}

	MusicalMode operator= (MusicalMode const & other);

	virtual std::string name() const { return _name; }
	TuningSystem tuning() const { return _tuning; }
	MusicalModeType type() const { return _type; }
	int size() const { return _elements.size(); }
	std::vector<float> const & elements() const { return _elements; }
	int ring_id () const;

	std::vector<int> as_midi (int scale_root) const;
	void set_name (std::string const & str);

	PBD::Signal<void()> NameChanged;
	PBD::Signal<void()> Changed;

	static std::multimap<TuningSystem,MusicalMode> scales_by_tuning;
	static std::map<std::string,MusicalMode> scales_by_name;
	static std::map<int,MusicalMode> scales_by_id;

	static std::map<TuningSystem,int> tone_equivalent_ratio;
	static std::map<TuningSystem,int> tones_per_equivalent;

   protected:
        TuningSystem _tuning;
	mutable int _ring_id;
	std::string _name;
	MusicalModeType _type;
	std::vector<float> _elements;

	static void init ();

	std::vector<int> absolute_pitch_as_midi (int root) const;
	std::vector<int> pitch_class_as_midi (int root) const;
	std::vector<int> wholetone_steps_as_midi (int root) const;
	std::vector<int> ratio_steps_as_midi (int root) const;
	std::vector<int> ratio_from_root_as_midi (int root) const;
	std::vector<int> midi_note_as_midi (int root) const;
};


class LIBARDOUR_API MusicalKey : public MusicalMode
{
    public:
	MusicalKey (float root, MusicalMode const &);
	MusicalKey (float root, std::string const &);
	MusicalKey (MusicalKey const & other);

	std::string name() const;
	std::string mode_name() const;
	float root() const { return _root; }
	float nth (unsigned int n) const;
	bool  in_key (int midi_note) const;
	int   closest_midi_note (int midi_note) const;
	int   lower_midi_note (int midi_note) const;
	int   higher_midi_note (int midi_note) const;
	int   conform_midi_note (int midi_note, ARDOUR::KeyEnforcementPolicy) const;

   private:
	float _root;
	mutable std::vector<int> _midi_notes;
};

} // namespace
