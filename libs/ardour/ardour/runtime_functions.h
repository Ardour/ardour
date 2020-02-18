/*
 * Copyright (C) 2007-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#ifndef __ardour_runtime_functions_h__
#define __ardour_runtime_functions_h__

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

	typedef float (*compute_peak_t)          (const ARDOUR::Sample *, pframes_t, float);
	typedef void  (*find_peaks_t)            (const ARDOUR::Sample *, pframes_t, float *, float*);
	typedef void  (*apply_gain_to_buffer_t)  (ARDOUR::Sample *, pframes_t, float);
	typedef void  (*mix_buffers_with_gain_t) (ARDOUR::Sample *, const ARDOUR::Sample *, pframes_t, float);
	typedef void  (*mix_buffers_no_gain_t)   (ARDOUR::Sample *, const ARDOUR::Sample *, pframes_t);
	typedef void  (*copy_vector_t)           (ARDOUR::Sample *, const ARDOUR::Sample *, pframes_t);

	LIBARDOUR_API extern compute_peak_t          compute_peak;
	LIBARDOUR_API extern find_peaks_t            find_peaks;
	LIBARDOUR_API extern apply_gain_to_buffer_t  apply_gain_to_buffer;
	LIBARDOUR_API extern mix_buffers_with_gain_t mix_buffers_with_gain;
	LIBARDOUR_API extern mix_buffers_no_gain_t   mix_buffers_no_gain;
	LIBARDOUR_API extern copy_vector_t           copy_vector;
}

#endif /* __ardour_runtime_functions_h__ */
