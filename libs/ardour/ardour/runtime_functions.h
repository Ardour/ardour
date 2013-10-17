/*
    Copyright (C) 2007 Paul Davis

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

#ifndef __ardour_runtime_functions_h__
#define __ardour_runtime_functions_h__

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

	typedef float (*compute_peak_t)			(const ARDOUR::Sample *, pframes_t, float);
	typedef void  (*find_peaks_t)                   (const ARDOUR::Sample *, pframes_t, float *, float*);
	typedef void  (*apply_gain_to_buffer_t)		(ARDOUR::Sample *, pframes_t, float);
	typedef void  (*mix_buffers_with_gain_t)	(ARDOUR::Sample *, const ARDOUR::Sample *, pframes_t, float);
	typedef void  (*mix_buffers_no_gain_t)		(ARDOUR::Sample *, const ARDOUR::Sample *, pframes_t);

	extern compute_peak_t		compute_peak;
	extern find_peaks_t             find_peaks;
	extern apply_gain_to_buffer_t	apply_gain_to_buffer;
	extern mix_buffers_with_gain_t	mix_buffers_with_gain;
	extern mix_buffers_no_gain_t	mix_buffers_no_gain;
}

#endif /* __ardour_runtime_functions_h__ */
