/*
 * Copyright (C) 2012 Paul Davis <paul@linuxaudiosystems.com>
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

#include "temporal/types.h"
#include "ardour/interthread_info.h"

namespace ARDOUR {

	struct TimeFXRequest : public InterThreadInfo {

	enum Algorithm {
		Rubberband,
		StaffPad,
		SoundTouch,
	};

		TimeFXRequest()
			: algorithm (Rubberband)
			, time_fraction(0,1)
			, pitch_fraction(0)
			, quick_seek(false)
			, antialias(false)
			, opts(0) {}

		Algorithm algorithm;
		Temporal::ratio_t time_fraction;
		float pitch_fraction;
		/* SoundTouch */
		bool  quick_seek;
		bool  antialias;
		/* RubberBand */
		int   opts; // really RubberBandStretcher::Options
	};
}

