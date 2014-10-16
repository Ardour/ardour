#ifndef AUDIOGRAPHER_NORMALIZER_H
#define AUDIOGRAPHER_NORMALIZER_H

#include "audiographer/visibility.h"
#include "audiographer/sink.h"
#include "audiographer/routines.h"
#include "audiographer/utils/listed_source.h"

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
	Normalizer (float target_dB);
	~Normalizer();

	/// Sets the peak found in the material to be normalized \see PeakReader \n RT safe
	void set_peak (float peak);

	/** Allocates a buffer for using with const ProcessContexts
	  * This function does not need to be called if
	  * non-const ProcessContexts are given to \a process() .
	  * \n Not RT safe
	  */
	void alloc_buffer(framecnt_t frames);

	/// Process a const ProcessContext \see alloc_buffer() \n RT safe
	void process (ProcessContext<float> const & c);
	
	/// Process a non-const ProcsesContext in-place \n RT safe
	void process (ProcessContext<float> & c);

private:
	bool      enabled;
	float     target;
	float     gain;
	
	float *   buffer;
	framecnt_t buffer_size;
};


} // namespace

#endif // AUDIOGRAPHER_NORMALIZER_H
