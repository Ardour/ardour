#ifndef AUDIOGRAPHER_PEAK_READER_H
#define AUDIOGRAPHER_PEAK_READER_H

#include "audiographer/visibility.h"
#include "audiographer/sink.h"
#include "audiographer/routines.h"
#include "audiographer/utils/listed_source.h"

namespace AudioGrapher
{

/// A class that reads the maximum value from a stream
class LIBAUDIOGRAPHER_API PeakReader : public ListedSource<float>, public Sink<float>
{
  public:
	/// Constructor \n RT safe
	PeakReader() : peak (0.0) {}
	
	/// Returns the highest absolute of the values found so far. \n RT safe
	float get_peak() { return peak; }
	
	/// Resets the peak to 0 \n RT safe
	void  reset() { peak = 0.0; }

	/// Finds peaks from the data \n RT safe
	void process (ProcessContext<float> const & c)
	{
		peak = Routines::compute_peak (c.data(), c.frames(), peak);
		ListedSource<float>::output(c);
	}
	
	using Sink<float>::process;
	
  private:
	float peak;
};


} // namespace

#endif // AUDIOGRAPHER_PEAK_READER_H
