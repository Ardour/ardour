#ifndef AUDIOGRAPHER_ROUTINES_H
#define AUDIOGRAPHER_ROUTINES_H

#include "types.h"

#include <cmath>

#include "audiographer/visibility.h"

namespace AudioGrapher
{

/// Allows overriding some routines with more efficient ones.
class LIBAUDIOGRAPHER_API Routines
{
  public:
	typedef uint32_t uint_type;
	
	typedef float (*compute_peak_t)          (float const *, uint_type, float);
	typedef void  (*apply_gain_to_buffer_t)  (float *, uint_type, float);
	
	static void override_compute_peak         (compute_peak_t func)         { _compute_peak = func; }
	static void override_apply_gain_to_buffer (apply_gain_to_buffer_t func) { _apply_gain_to_buffer = func; }
	
	/** Computes peak in float buffer
	  * \n RT safe
	  * \param data buffer from which the peak is computed
	  * \param frames length of the portion of \a buffer that is checked
	  * \param current_peak current peak of buffer, if calculated in several passes
	  * \return maximum of values in [\a data, \a data + \a frames) and \a current_peak
	  */
	static inline float compute_peak (float const * data, uint_type frames, float current_peak)
	{
		return (*_compute_peak) (data, frames, current_peak);
	}

	/** Applies constant gain to buffer
	 * \n RT safe
	 * \param data data to which the gain is applied
	 * \param frames length of data
	 * \param gain gain that is applied
	 */
	static inline void apply_gain_to_buffer (float * data, uint_type frames, float gain)
	{
		(*_apply_gain_to_buffer) (data, frames, gain);
	}

  private:
	static inline float default_compute_peak (float const * data, uint_type frames, float current_peak)
	{
		for (uint_type i = 0; i < frames; ++i) {
			float abs = std::fabs(data[i]);
			if (abs > current_peak) { current_peak = abs; }
		}
		return current_peak;
	}

	static inline void default_apply_gain_to_buffer (float * data, uint_type frames, float gain)
	{
		for (uint_type i = 0; i < frames; ++i) {
			data[i] *= gain;
		}
	}
	
	static compute_peak_t          _compute_peak;
	static apply_gain_to_buffer_t  _apply_gain_to_buffer;
};

} // namespace

#endif // AUDIOGRAPHER_ROUTINES_H
