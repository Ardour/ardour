/*
    Copyright (C) 2009 Paul Davis
    Author: David Robillard

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: midiregion.h 733 2006-08-01 17:19:38Z drobilla $
*/

#include "pbd/stacktrace.h"

#include "ardour/beats_frames_converter.h"
#include "ardour/tempo.h"

namespace ARDOUR {

/** Takes a positive duration in quarter-note beats and considers it as a distance from the origin
 *  supplied to the constructor.  Returns the equivalent number of frames,
 *  taking tempo changes into account.
 */
framepos_t
BeatsFramesConverter::to (Evoral::Beats beats) const
{
	if (beats < Evoral::Beats()) {
		std::cerr << "negative beats passed to BFC: " << beats << std::endl;
		PBD::stacktrace (std::cerr, 30);
		return 0;
	}
	return _tempo_map.framepos_plus_qn (_origin_b, beats) - _origin_b;
}

/** Takes a duration in frames and considers it as a distance from the origin
 *  supplied to the constructor.  Returns the equivalent number of quarter-note beats,
 *  taking tempo changes into account.
 */
Evoral::Beats
BeatsFramesConverter::from (framepos_t frames) const
{
	return _tempo_map.framewalk_to_qn (_origin_b, frames);
}

/** As above, but with quarter-note beats in double instead (for GUI). */
framepos_t
DoubleBeatsFramesConverter::to (double beats) const
{
	if (beats < 0.0) {
		std::cerr << "negative beats passed to BFC: " << beats << std::endl;
		PBD::stacktrace (std::cerr, 30);
		return 0;
	}
	return _tempo_map.framepos_plus_qn (_origin_b, Evoral::Beats(beats)) - _origin_b;
}

/** As above, but with quarter-note beats in double instead (for GUI). */
double
DoubleBeatsFramesConverter::from (framepos_t frames) const
{
	return _tempo_map.framewalk_to_qn (_origin_b, frames).to_double();
}

} /* namespace ARDOUR */

