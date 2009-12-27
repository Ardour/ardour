#ifndef AUDIOGRAPHER_SR_CONVERTER_H
#define AUDIOGRAPHER_SR_CONVERTER_H

#include <samplerate.h>

#include "types.h"
#include "listed_source.h"
#include "sink.h"

namespace AudioGrapher
{

class SampleRateConverter : public ListedSource<float>, public Sink<float>
{
  public:
	SampleRateConverter (uint32_t channels);
	~SampleRateConverter ();

	// not RT safe
	void init (nframes_t in_rate, nframes_t out_rate, int quality = 0);
	
	// returns max amount of frames that will be output
	nframes_t allocate_buffers (nframes_t max_frames);
	
	// could be RT safe (check libsamplerate to be sure)
	void process (ProcessContext<float> const & c);
	using Sink<float>::process;

  private:

	void set_end_of_input (ProcessContext<float> const & c);
	void reset ();

	bool           active;
	uint32_t       channels;
	nframes_t      max_frames_in;
	
	float *        leftover_data;
	nframes_t      leftover_frames;
	nframes_t      max_leftover_frames;

	float *        data_out;
	nframes_t      data_out_size;

	SRC_DATA       src_data;
	SRC_STATE*     src_state;
};

} // namespace

#endif // AUDIOGRAPHER_SR_CONVERTER_H
