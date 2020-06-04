#ifndef AUDIOGRAPHER_DEMO_NOISE_H
#define AUDIOGRAPHER_DEMO_NOISE_H

#include "audiographer/visibility.h"
#include "audiographer/sink.h"
#include "audiographer/types.h"
#include "audiographer/utils/listed_source.h"

namespace AudioGrapher
{

/// Noise injector
class LIBAUDIOGRAPHER_API DemoNoiseAdder
  : public ListedSource<float>
  , public Sink<float>
  , public Throwing<>
{
public:
	/// Constructor. \n RT safe
	DemoNoiseAdder (unsigned int channels);
	~DemoNoiseAdder ();

	void init (samplecnt_t max_samples, samplecnt_t interval, samplecnt_t duration, float level);

	void process (ProcessContext<float> const& ctx);
	using Sink<float>::process;

private:
	float*       _data_out;
	samplecnt_t  _data_out_size;

	unsigned int _channels;
	samplecnt_t  _interval;
	samplecnt_t  _duration;
	float        _level;
	samplecnt_t  _pos;

	/* Park-Miller-Carta */
	uint32_t randi ();
	float    randf () { return (randi () / 1073741824.f) - 1.f; }
	uint32_t _rseed;
};

} // namespace

#endif
