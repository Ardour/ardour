#ifndef AUDIOGRAPHER_LIMITER_H
#define AUDIOGRAPHER_LIMITER_H

#include "ardour/export_analysis.h"

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

	void set_duration (samplecnt_t);
	void set_result (ARDOUR::ExportAnalysisPtr);

	void process (ProcessContext<float> const& ctx);
	using Sink<float>::process;

private:
	void stats (samplecnt_t);

	bool        _enabled;
	float*      _buf;
	samplecnt_t _size;
	samplecnt_t _latency;

	samplecnt_t _cnt;
	samplecnt_t _spp;
	size_t      _pos;

	ARDOUR::ExportAnalysisPtr _result;
	AudioGrapherDSP::Limiter  _limiter;
};

} // namespace

#endif
