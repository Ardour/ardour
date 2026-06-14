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

#include <vector>
#include <string>
#include <map>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class ScaleProvider;

class LIBARDOUR_API ChordProvider
{
  public:
	ChordProvider () {}
	virtual ~ChordProvider() {}

	typedef std::vector<int> Intervals;

	virtual bool get_midi_chord (int root_pitch, Intervals& pitches, bool& arpeggiate) const = 0;

	struct ChordInfo {
		Intervals intervals;
		int64_t hashed;
		std::string canonical_name;
		std::string short_name;
		std::vector<std::string> other_names;

		ChordInfo (Intervals const & i, int64_t hash, std::string const & canon, std::string const & shrt, std::vector<std::string> const & others)
			: intervals (i)
			, hashed (hash)
			, canonical_name (canon)
			, short_name (shrt)
			, other_names (others) {}
	};

	ChordInfo const * by_short_name (std::string const &) const;
	ChordInfo const * by_canonical_name (std::string const &) const;
	ChordInfo const * by_any_name (std::string const &) const;

	static std::vector<ChordInfo> chord_info;
	static void load_12tet_chords ();
	static int64_t hash_intervals (ChordProvider::Intervals const & intervals);
	static bool add_chord (ChordInfo const &);

	static int load (std::string const & path);
	static int save ();

	std::string identify_chord (Intervals const &);
	std::string canonical_name (Intervals const &);
	std::string short_name (Intervals const &);
	std::vector<std::string> other_names (Intervals const &);

	enum TET12Intervals {
		Unison = 0,
		MinorSecond = 1,
		MajorSecond = 2,
		MinorThird = 3,
		MajorThird = 4,
		PerfectFourth = 5,
		Tritone = 6,
		PerfectFifth = 7,
		MinorSixth = 8,
		MajorSixth = 9,
		MinorSeventh = 10,
		MajorSeventh = 11,
		PerfectOctave = 12,

		/* aliases set #1 */

		P0 = PerfectOctave,
		m2 = MinorSecond,
		M2 = MajorSecond,
		m3 = MinorThird,
		M3 = MajorThird,
		P4 = PerfectFourth,
		A4 = Tritone,
		d5 = Tritone,
		P5 = PerfectFifth,
		m6 = MinorSixth,
		M6 = MajorSixth,
		m7 = MinorSeventh,
		M7 = MajorSeventh,
		P8 = PerfectOctave,

		/* aliases set two */

		flat2 = MinorSecond,
		fourth = PerfectFourth,
		flat5 = Tritone,
		sharp5 = MinorSixth,
		dom7 = MinorSeventh,
		dblflat7 = MajorSixth,

		/* aliases set three */

		AugmentedFifth = 8,
		DiminishedFifth = 6,
		DiminishedSeventh = 9,

		/* aliases set four */

		MajorNinth = 14, // Octave + 2
		M9 = 14,
		PerfectEleventh = 17,
		P11 = 17,
		MajorThirteenth = 21, // Octave + 9
		M13 = 21,

	};
};

}
