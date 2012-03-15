/*
    Copyright (C) 2002 Paul Davis

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

*/

#ifndef __libardour_timefx_request_h__
#define __libardour_timefx_request_h__

#include "ardour/interthread_info.h"

namespace ARDOUR {

	struct TimeFXRequest : public InterThreadInfo {
		TimeFXRequest()
			: time_fraction(0), pitch_fraction(0),
			quick_seek(false), antialias(false),  opts(0) {}
		float time_fraction;
		float pitch_fraction;
		/* SoundTouch */
		bool  quick_seek;
		bool  antialias;
		/* RubberBand */
		int   opts; // really RubberBandStretcher::Options
	};
}

#endif /* __libardour_timefx_request_h__ */
