/*
 * Copyright (C) 1999-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_mode_h__
#define __ardour_mode_h__

#include <vector>

class MusicalMode
{
  public:
	enum Type {
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

	MusicalMode (Type t);
	~MusicalMode ();

	std::vector<float> steps;

  private:
	static void fill (MusicalMode&, Type);
};

#endif /* __ardour_mode_h__ */
