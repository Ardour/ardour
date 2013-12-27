#ifndef AUDIOGRAPHER_INTERLEAVER_H
#define AUDIOGRAPHER_INTERLEAVER_H

#include "audiographer/visibility.h"
#include "audiographer/types.h"
#include "audiographer/sink.h"
#include "audiographer/exception.h"
#include "audiographer/throwing.h"
#include "audiographer/utils/listed_source.h"

#include <vector>
#include <cmath>

namespace AudioGrapher
{

/// Interleaves many streams of non-interleaved data into one interleaved stream
template<typename T = DefaultSampleType>
class LIBAUDIOGRAPHER_API Interleaver
  : public ListedSource<T>
  , public Throwing<>
{
  public: 
	
	/// Constructs an interleaver \n RT safe
	Interleaver()
	  : channels (0)
	  , max_frames (0)
	  , buffer (0)
	{}
	
	~Interleaver() { reset(); }
	
	/// Inits the interleaver. Must be called before using. \n Not RT safe
	void init (unsigned int num_channels, framecnt_t max_frames_per_channel)
	{
		reset();
		channels = num_channels;
		max_frames = max_frames_per_channel;
		
		buffer = new T[channels * max_frames];
		
		for (unsigned int i = 0; i < channels; ++i) {
			inputs.push_back (InputPtr (new Input (*this, i)));
		}
	}
	
	/** Returns the input indexed by \a channel \n RT safe
	  * \n The \a process function of returned Sinks are also RT Safe
	  */
	typename Source<T>::SinkPtr input (unsigned int channel)
	{
		if (throw_level (ThrowObject) && channel >= channels) {
			throw Exception (*this, "Channel out of range");
		}
		
		return boost::static_pointer_cast<Sink<T> > (inputs[channel]);
	}
	
  private: 
 
	class Input : public Sink<T>
	{
	  public:
		Input (Interleaver & parent, unsigned int channel)
		  : frames_written (0), parent (parent), channel (channel) {}
		
		void process (ProcessContext<T> const & c)
		{
			if (parent.throw_level (ThrowProcess) && c.channels() > 1) {
				throw Exception (*this, "Data input has more than on channel");
			}
			if (parent.throw_level (ThrowStrict) && frames_written) {
				throw Exception (*this, "Input channels out of sync");
			}
			frames_written = c.frames();
			parent.write_channel (c, channel);
		}
		
		using Sink<T>::process;
		
		framecnt_t frames() { return frames_written; }
		void reset() { frames_written = 0; }
		
	  private:
		framecnt_t frames_written;
		Interleaver & parent;
		unsigned int channel;
	};
	
	void reset ()
	{
		inputs.clear();
		delete [] buffer;
		buffer = 0;
		channels = 0;
		max_frames = 0;
	}
	
	void reset_channels ()
	{
		for (unsigned int i = 0; i < channels; ++i) {
			inputs[i]->reset();
		}

	}
	
	void write_channel (ProcessContext<T> const & c, unsigned int channel)
	{
		if (throw_level (ThrowProcess) && c.frames() > max_frames) {
			reset_channels();
			throw Exception (*this, "Too many frames given to an input");
		}
		
		for (unsigned int i = 0; i < c.frames(); ++i) {
			buffer[channel + (channels * i)] = c.data()[i];
		}
		
		framecnt_t const ready_frames = ready_to_output();
		if (ready_frames) {
			ProcessContext<T> c_out (c, buffer, ready_frames, channels);
			ListedSource<T>::output (c_out);
			reset_channels ();
		}
	}

	framecnt_t ready_to_output()
	{
		framecnt_t ready_frames = inputs[0]->frames();
		if (!ready_frames) { return 0; }

		for (unsigned int i = 1; i < channels; ++i) {
			framecnt_t const frames = inputs[i]->frames();
			if (!frames) { return 0; }
			if (throw_level (ThrowProcess) && frames != ready_frames) {
				init (channels, max_frames);
				throw Exception (*this, "Frames count out of sync");
			}
		}
		return ready_frames * channels;
	}

	typedef boost::shared_ptr<Input> InputPtr;
	std::vector<InputPtr> inputs;
	
	unsigned int channels;
	framecnt_t max_frames;
	T * buffer;
};

} // namespace

#endif // AUDIOGRAPHER_INTERLEAVER_H
