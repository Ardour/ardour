#ifndef AUDIOGRAPHER_NORMALIZER_H
#define AUDIOGRAPHER_NORMALIZER_H

#include "audiographer/visibility.h"
#include "audiographer/sink.h"
#include "audiographer/routines.h"
#include "audiographer/utils/listed_source.h"

#include <cstring>

namespace AudioGrapher
{

/// A class for normalizing to a specified target in dB
class LIBAUDIOGRAPHER_API Normalizer
  : public ListedSource<float>
  , public Sink<float>
  , public Throwing<>
{
  public:
	/// Constructs a normalizer with a specific target in dB \n RT safe
	Normalizer (float target_dB)
	  : enabled (false)
	  , buffer (0)
	  , buffer_size (0)
	{
		target = pow (10.0f, target_dB * 0.05f);
	}
	
	~Normalizer()
	{
		delete [] buffer;
	}

	/// Sets the peak found in the material to be normalized \see PeakReader \n RT safe
	void set_peak (float peak)
	{
		if (peak == 0.0f || peak == target) {
			/* don't even try */
			enabled = false;
		} else {
			enabled = true;
			gain = target / peak;
		}
	}

	/** Allocates a buffer for using with const ProcessContexts
	  * This function does not need to be called if
	  * non-const ProcessContexts are given to \a process() .
	  * \n Not RT safe
	  */
	void alloc_buffer(framecnt_t frames)
	{
		delete [] buffer;
		buffer = new float[frames];
		buffer_size = frames;
	}

	/// Process a const ProcessContext \see alloc_buffer() \n RT safe
	void process (ProcessContext<float> const & c)
	{
		if (throw_level (ThrowProcess) && c.frames() > buffer_size) {
			throw Exception (*this, "Too many frames given to process()");
		}
		
		if (enabled) {
			memcpy (buffer, c.data(), c.frames() * sizeof(float));
			Routines::apply_gain_to_buffer (buffer, c.frames(), gain);
		}
		
		ProcessContext<float> c_out (c, buffer);
		ListedSource<float>::output (c_out);
	}
	
	/// Process a non-const ProcsesContext in-place \n RT safe
	void process (ProcessContext<float> & c)
	{
		if (enabled) {
			Routines::apply_gain_to_buffer (c.data(), c.frames(), gain);
		}
		ListedSource<float>::output(c);
	}
	
  private:
	bool      enabled;
	float     target;
	float     gain;
	
	float *   buffer;
	framecnt_t buffer_size;
};


} // namespace

#endif // AUDIOGRAPHER_NORMALIZER_H
