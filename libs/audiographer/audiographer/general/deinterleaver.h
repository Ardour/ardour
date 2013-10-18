#ifndef AUDIOGRAPHER_DEINTERLEAVER_H
#define AUDIOGRAPHER_DEINTERLEAVER_H

#include "audiographer/visibility.h"
#include "audiographer/types.h"
#include "audiographer/source.h"
#include "audiographer/sink.h"
#include "audiographer/exception.h"
#include "audiographer/utils/identity_vertex.h"

#include <vector>

namespace AudioGrapher
{

/// Converts on stream of interleaved data to many streams of uninterleaved data.
template<typename T = DefaultSampleType>
class LIBAUDIOGRAPHER_API DeInterleaver
  : public Sink<T>
  , public Throwing<>
{
  private:
	typedef boost::shared_ptr<IdentityVertex<T> > OutputPtr;
	
  public:
	/// Constructor. \n RT safe
	DeInterleaver()
	  : channels (0)
	  , max_frames (0)
	  , buffer (0)
	{}
	
	~DeInterleaver() { reset(); }
	
	typedef boost::shared_ptr<Source<T> > SourcePtr;
	
	/// Inits the deinterleaver. Must be called before using. \n Not RT safe
	void init (unsigned int num_channels, framecnt_t max_frames_per_channel)
	{
		reset();
		channels = num_channels;
		max_frames = max_frames_per_channel;
		buffer = new T[max_frames];
		
		for (unsigned int i = 0; i < channels; ++i) {
			outputs.push_back (OutputPtr (new IdentityVertex<T>));
		}
	}
	
	/// Returns an output indexed by \a channel \n RT safe
	SourcePtr output (unsigned int channel)
	{
		if (throw_level (ThrowObject) && channel >= channels) {
			throw Exception (*this, "channel out of range");
		}
		
		return outputs[channel];
	}
	
	/// Deinterleaves data and outputs it to the outputs. \n RT safe
	void process (ProcessContext<T> const & c)
	{
		framecnt_t frames = c.frames();
		T const * data = c.data();
		
		framecnt_t const  frames_per_channel = frames / channels;
		
		if (throw_level (ThrowProcess) && c.channels() != channels) {
			throw Exception (*this, "wrong amount of channels given to process()");
		}
		
		if (throw_level (ThrowProcess) && frames_per_channel > max_frames) {
			throw Exception (*this, "too many frames given to process()");
		}
		
		unsigned int channel = 0;
		for (typename std::vector<OutputPtr>::iterator it = outputs.begin(); it != outputs.end(); ++it, ++channel) {
			if (!*it) { continue; }
			
			for (unsigned int i = 0; i < frames_per_channel; ++i) {
				buffer[i] = data[channel + (channels * i)];
			}
			
			ProcessContext<T> c_out (c, buffer, frames_per_channel, 1);
			(*it)->process (c_out);
		}
	}
	
	using Sink<T>::process;
	
  private:

	void reset ()
	{
		outputs.clear();
		delete [] buffer;
		buffer = 0;
		channels = 0;
		max_frames = 0;
	}
	
	std::vector<OutputPtr> outputs;
	unsigned int channels;
	framecnt_t max_frames;
	T * buffer;
};

} // namespace

#endif // AUDIOGRAPHER_DEINTERLEAVER_H
