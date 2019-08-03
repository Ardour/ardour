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

#include "ardour/beats_samples_converter.h"
#include "ardour/tempo.h"

namespace ARDOUR {

/** Takes a positive duration in quarter-note beats and considers it as a distance from the origin
 *  supplied to the constructor.  Returns the equivalent number of samples,
 *  taking tempo changes into account.
 */
samplepos_t
BeatsSamplesConverter::to (Temporal::Beats beats) const
{
	if (beats < Temporal::Beats()) {
		std::cerr << "negative beats passed to BFC: " << beats << std::endl;
		PBD::stacktrace (std::cerr, 30);
		return 0;
	}
	return _tempo_map.samplepos_plus_qn (_origin_b, beats) - _origin_b;
}

/** Takes a duration in samples and considers it as a distance from the origin
 *  supplied to the constructor.  Returns the equivalent number of quarter-note beats,
 *  taking tempo changes into account.
 */
Temporal::Beats
BeatsSamplesConverter::from (samplepos_t samples) const
{
	return _tempo_map.framewalk_to_qn (_origin_b, samples);
}

/** As above, but with quarter-note beats in double instead (for GUI). */
samplepos_t
DoubleBeatsSamplesConverter::to (double beats) const
{
	if (beats < 0.0) {
		std::cerr << "negative beats passed to BFC: " << beats << std::endl;
		PBD::stacktrace (std::cerr, 30);
		return 0;
	}
	return _tempo_map.samplepos_plus_qn (_origin_b, Temporal::Beats(beats)) - _origin_b;
}

/** As above, but with quarter-note beats in double instead (for GUI). */
double
DoubleBeatsSamplesConverter::from (samplepos_t samples) const
{
	return _tempo_map.framewalk_to_qn (_origin_b, samples).to_double();
}

} /* namespace ARDOUR */

