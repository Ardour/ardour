#ifndef AUDIOGRAPHER_ROUTINES_H
#define AUDIOGRAPHER_ROUTINES_H

#include "types.h"

#include <cmath>

namespace AudioGrapher
{

class Routines
{
  public:
	typedef float (*compute_peak_t)          (float const *, nframes_t, float);
	typedef void  (*apply_gain_to_buffer_t)  (float *, nframes_t, float);
	
	static void override_compute_peak         (compute_peak_t func)         { _compute_peak = func; }
	static void override_apply_gain_to_buffer (apply_gain_to_buffer_t func) { _apply_gain_to_buffer = func; }
	
	static inline float compute_peak (float const * data, nframes_t frames, float current_peak)
	{
		return (*_compute_peak) (data, frames, current_peak);
	}

	static inline void apply_gain_to_buffer (float * data, nframes_t frames, float gain)
	{
		(*_apply_gain_to_buffer) (data, frames, gain);
	}

  private:
	static inline float default_compute_peak (float const * data, nframes_t frames, float current_peak)
	{
		for (nframes_t i = 0; i < frames; ++i) {
			float abs = std::fabs(data[i]);
			if (abs > current_peak) { current_peak = abs; }
		}
		return current_peak;
	}

	static inline void default_apply_gain_to_buffer (float * data, nframes_t frames, float gain)
	{
		for (nframes_t i = 0; i < frames; ++i) {
			data[i] *= gain;
		}
	}
	
	static compute_peak_t          _compute_peak;
	static apply_gain_to_buffer_t  _apply_gain_to_buffer;
};

} // namespace

#endif // AUDIOGRAPHER_ROUTINES_H
