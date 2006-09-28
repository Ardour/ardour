#ifndef __ardour_peak_h__
#define __ardour_peak_h__

#include <cmath>
#include <ardour/types.h>
#include <ardour/utils.h>

static inline float
compute_peak (ARDOUR::Sample *buf, nframes_t nsamples, float current) 
{
	for (nframes_t i = 0; i < nsamples; ++i) {
		current = f_max (current, fabsf (buf[i]));
	}
	return current;
}	

#endif /* __ardour_peak_h__ */
