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

#include "pbd/stacktrace.h"

#include "temporal/tempo.h"

#include "ardour/beats_samples_converter.h"

namespace ARDOUR {

/** Takes a positive duration in quarter-note beats and considers it as a distance from the origin
 *  supplied to the constructor. Returns the equivalent number of samples,
 *  taking tempo changes into account.
 */
samplecnt_t
BeatsSamplesConverter::to (Temporal::Beats beats) const
{
	if (beats < Temporal::Beats()) {
		std::cerr << "negative beats passed to BFC: " << beats << std::endl;
		PBD::stacktrace (std::cerr, 30);
		return 0;
	}
	return _tempo_map.sample_quarters_delta_as_samples (_origin, beats) - _origin;
}

/** Takes a positive duration in superclocks and considers it as a distance from the origin
 *  supplied to the constructor. Returns the equivalent number of quarter-note beats,
 *  taking tempo changes into account.
 *
 *  Distance must be positive because we assume we are walking forward from our origin.
 */
Temporal::Beats
BeatsSamplesConverter::from (samplecnt_t distance) const
{
	return _tempo_map.sample_delta_as_quarters (_origin, distance);
}
