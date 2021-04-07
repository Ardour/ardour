#ifndef AUDIOGRAPHER_LIMITER_H
#define AUDIOGRAPHER_LIMITER_H

#include "audiographer/visibility.h"
#include "audiographer/sink.h"
#include "audiographer/utils/listed_source.h"

#include "private/limiter/limiter.h"

namespace AudioGrapher
{

class LIBAUDIOGRAPHER_API Limiter
  : public ListedSource<float>
  , public Sink<float>
  , public Throwing<>
{
public:
	Limiter (float sample_rate, unsigned int channels, samplecnt_t);
	~Limiter ();

	void set_input_gain (float dB);
	void set_threshold (float dB);
	void set_release (float s);

	void process (ProcessContext<float> const& ctx);
	using Sink<float>::process;

private:
	bool        _enabled;
	float*      _buf;
	samplecnt_t _size;
	samplecnt_t _latency;

	AudioGrapherDSP::Limiter _limiter;
};

} // namespace

#endif
