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

#include <evoral/TimeConverter.hpp>
#include <ardour/types.h>

#ifndef __ardour_beats_frames_converter_h__
#define __ardour_beats_frames_converter_h__

namespace ARDOUR {

class Session;

class BeatsFramesConverter : public Evoral::TimeConverter<double,sframes_t> {
public:
	BeatsFramesConverter(Session& session, sframes_t origin)
		: _session(session)
		, _origin(origin)
	{}
	
	sframes_t to(double beats)       const;
	double    from(sframes_t frames) const;

	sframes_t origin() const              { return _origin; }
	void     set_origin(sframes_t origin) { _origin = origin; }

private:
	Session&  _session;
	sframes_t _origin;
};

} /* namespace ARDOUR */

#endif /* __ardour_beats_frames_converter_h__ */
