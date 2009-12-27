#ifndef AUDIOGRAPHER_NORMALIZER_H
#define AUDIOGRAPHER_NORMALIZER_H

#include "listed_source.h"
#include "sink.h"
#include "routines.h"

#include <cstring>

namespace AudioGrapher
{

class Normalizer : public ListedSource<float>, Sink<float>
{
  public:
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

	void alloc_buffer(nframes_t frames)
	{
		delete [] buffer;
		buffer = new float[frames];
		buffer_size = frames;
	}

	void process (ProcessContext<float> const & c)
	{
		if (c.frames() > buffer_size) {
			throw Exception (*this, "Too many frames given to process()");
		}
		
		if (enabled) {
			memcpy (buffer, c.data(), c.frames() * sizeof(float));
			Routines::apply_gain_to_buffer (buffer, c.frames(), gain);
		}
		
		ProcessContext<float> c_out (c, buffer);
		ListedSource<float>::output (c_out);
	}
	
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
	nframes_t buffer_size;
};


} // namespace

#endif // AUDIOGRAPHER_NORMALIZER_H
