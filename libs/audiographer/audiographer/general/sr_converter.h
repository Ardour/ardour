#ifndef AUDIOGRAPHER_SR_CONVERTER_H
#define AUDIOGRAPHER_SR_CONVERTER_H

#include <samplerate.h>

#include "audiographer/visibility.h"
#include "audiographer/flag_debuggable.h"
#include "audiographer/sink.h"
#include "audiographer/throwing.h"
#include "audiographer/types.h"
#include "audiographer/utils/listed_source.h"

namespace AudioGrapher
{

/// Samplerate converter
class LIBAUDIOGRAPHER_API SampleRateConverter
  : public ListedSource<float>
  , public Sink<float>
  , public FlagDebuggable<>
  , public Throwing<>
{
  public:
	/// Constructor. \n RT safe
	SampleRateConverter (uint32_t channels);
	~SampleRateConverter ();

	/// Init converter \n Not RT safe
	void init (samplecnt_t in_rate, samplecnt_t out_rate, int quality = 0);

	/// Returns max amount of samples that will be output \n RT safe
	samplecnt_t allocate_buffers (samplecnt_t max_samples);

	/** Does sample rate conversion.
	  * Note that outpt size may vary a lot.
	  * May or may not output several contexts of data.
	  * \n Should be RT safe.
	  */
	void process (ProcessContext<float> const & c);
	using Sink<float>::process;

  private:

	void set_end_of_input (ProcessContext<float> const & c);
	void reset ();

	bool           active;
	uint32_t       channels;
	samplecnt_t     max_samples_in;

	float *        leftover_data;
	samplecnt_t     leftover_samples;
	samplecnt_t     max_leftover_samples;

	float *        data_out;
	samplecnt_t     data_out_size;

	SRC_DATA       src_data;
	SRC_STATE*     src_state;
};

} // namespace

#endif // AUDIOGRAPHER_SR_CONVERTER_H
