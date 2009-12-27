#ifndef AUDIOGRAPHER_INTERLEAVER_H
#define AUDIOGRAPHER_INTERLEAVER_H

#include "types.h"
#include "listed_source.h"
#include "sink.h"
#include "exception.h"

#include <vector>
#include <cmath>

namespace AudioGrapher
{

template<typename T>
class Interleaver : public ListedSource<T>
{
  public: 
	
	Interleaver();
	~Interleaver() { reset(); }
	
	void init (unsigned int num_channels, nframes_t max_frames_per_channel);
	typename Source<T>::SinkPtr input (unsigned int channel);
	
  private: 
 
	class Input : public Sink<T>
	{
	  public:
		Input (Interleaver & parent, unsigned int channel)
		  : frames_written (0), parent (parent), channel (channel) {}
		
		void process (ProcessContext<T> const & c)
		{
			if (c.channels() > 1) { throw Exception (*this, "Data input has more than on channel"); }
			if (frames_written) { throw Exception (*this, "Input channels out of sync"); }
			frames_written = c.frames();
			parent.write_channel (c, channel);
		}
		
		using Sink<T>::process;
		
		nframes_t frames() { return frames_written; }
		void reset() { frames_written = 0; }
		
	  private:
		nframes_t frames_written;
		Interleaver & parent;
		unsigned int channel;
	};
	  
	void reset ();
	void reset_channels ();
	void write_channel (ProcessContext<T> const & c, unsigned int channel);
	nframes_t ready_to_output();
	void output();	

	typedef boost::shared_ptr<Input> InputPtr;
	std::vector<InputPtr> inputs;
	
	unsigned int channels;
	nframes_t max_frames;
	T * buffer;
};

#include "interleaver-inl.h"

} // namespace

#endif // AUDIOGRAPHER_INTERLEAVER_H
