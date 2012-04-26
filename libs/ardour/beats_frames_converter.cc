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

/** Takes a positive duration in beats and considers it as a distance from the origin
 *  supplied to the constructor.  Returns the equivalent number of frames,
 *  taking tempo changes into account.
 */
framepos_t
BeatsFramesConverter::to (double beats) const
{
	if (beats < 0) {
		std::cerr << "negative beats passed to BFC: " << beats << std::endl;
		PBD::stacktrace (std::cerr, 30);
	}
	assert (beats >= 0);
	framecnt_t r = _tempo_map.framepos_plus_beats (_origin_b, beats) - _origin_b;
	return r;
}

/** Takes a duration in frames and considers it as a distance from the origin
 *  supplied to the constructor.  Returns the equivalent number of beats,
 *  taking tempo changes into account.
 */
double
BeatsFramesConverter::from (framepos_t frames) const
{
	double b = _tempo_map.framewalk_to_beats (_origin_b, frames);
	return b;
}

} /* namespace ARDOUR */

