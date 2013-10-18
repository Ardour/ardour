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

#include "evoral/TimeConverter.hpp"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

#ifndef __ardour_beats_frames_converter_h__
#define __ardour_beats_frames_converter_h__

namespace ARDOUR {

class TempoMap;

/** Converter between beats and frames.  Takes distances in beats or frames
 *  from some origin (supplied to the constructor in frames), and converts
 *  them to the opposite unit, taking tempo changes into account.
 */
class LIBARDOUR_API BeatsFramesConverter : public Evoral::TimeConverter<double,framepos_t> {
public:
	BeatsFramesConverter (TempoMap& tempo_map, framepos_t origin)
		: Evoral::TimeConverter<double, framepos_t> (origin)
		, _tempo_map(tempo_map)
	{}

	framepos_t to (double beats)        const;
	double     from (framepos_t frames) const;

private:
	TempoMap& _tempo_map;
};

} /* namespace ARDOUR */

#endif /* __ardour_beats_frames_converter_h__ */
