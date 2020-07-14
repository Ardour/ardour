/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cstdlib>

#include "temporal/beats.h"
#include "evoral/TimeConverter.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

#ifndef __ardour_beats_samples_converter_h__
#define __ardour_beats_samples_converter_h__

namespace ARDOUR {

class TempoMap;

/** Converter between quarter-note beats and samples.  Takes distances in quarter-note beats or samples
 *  from some origin (supplied to the constructor in samples), and converts
 *  them to the opposite unit, taking tempo changes into account.
 */
class LIBARDOUR_API BeatsSamplesConverter
	: public Evoral::TimeConverter<Temporal::Beats,samplepos_t> {
public:
	BeatsSamplesConverter (const TempoMap& tempo_map, samplepos_t origin)
		: Evoral::TimeConverter<Temporal::Beats, samplepos_t> (origin)
		, _tempo_map(tempo_map)
	{}

	samplepos_t    to (Temporal::Beats beats) const;
	Temporal::Beats from (samplepos_t samples) const;

private:
	const TempoMap& _tempo_map;
};

/** Converter between quarter-note beats and samples.  Takes distances in quarter-note beats or samples
 *  from some origin (supplied to the constructor in samples), and converts
 *  them to the opposite unit, taking tempo changes into account.
 */
class LIBARDOUR_API DoubleBeatsSamplesConverter
	: public Evoral::TimeConverter<double,samplepos_t> {
public:
	DoubleBeatsSamplesConverter (const TempoMap& tempo_map, samplepos_t origin)
		: Evoral::TimeConverter<double, samplepos_t> (origin)
		, _tempo_map(tempo_map)
	{}

	samplepos_t          to (double beats) const;
	double from (samplepos_t samples) const;

private:
	const TempoMap& _tempo_map;
};


} /* namespace ARDOUR */

#endif /* __ardour_beats_samples_converter_h__ */
