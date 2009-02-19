/*
    Copyright (C) 2009 Paul Davis 
    Author: Dave Robillard

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

#include <ardour/audioengine.h>
#include <ardour/beats_frames_converter.h>
#include <ardour/session.h>
#include <ardour/tempo.h>

namespace ARDOUR {

sframes_t
BeatsFramesConverter::to(double beats) const
{
	// FIXME: assumes tempo never changes after origin
	const Tempo& tempo = _session.tempo_map().tempo_at(_origin);
	const double frames_per_beat = tempo.frames_per_beat(
			_session.engine().frame_rate(),
			_session.tempo_map().meter_at(_origin));

	return lrint(beats * frames_per_beat);
}

double
BeatsFramesConverter::from(sframes_t frames) const
{
	// FIXME: assumes tempo never changes after origin
	const Tempo& tempo = _session.tempo_map().tempo_at(_origin);
	const double frames_per_beat = tempo.frames_per_beat(
			_session.engine().frame_rate(),
			_session.tempo_map().meter_at(_origin));

	return frames / frames_per_beat;
}

} /* namespace ARDOUR */

