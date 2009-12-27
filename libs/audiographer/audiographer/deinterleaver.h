#ifndef AUDIOGRAPHER_DEINTERLEAVER_H
#define AUDIOGRAPHER_DEINTERLEAVER_H

#include "types.h"
#include "source.h"
#include "sink.h"
#include "identity_vertex.h"
#include "exception.h"

#include <vector>

namespace AudioGrapher
{

template<typename T>
class DeInterleaver : public Sink<T>
{
  private:
	typedef boost::shared_ptr<IdentityVertex<T> > OutputPtr;
	
  public:
	DeInterleaver();
	~DeInterleaver() { reset(); }
	
	typedef boost::shared_ptr<Source<T> > SourcePtr;
	
	void init (unsigned int num_channels, nframes_t max_frames_per_channel);
	SourcePtr output (unsigned int channel);
	void process (ProcessContext<T> const & c);
	using Sink<T>::process;
	
  private:

	void reset ();
	
	std::vector<OutputPtr> outputs;
	unsigned int channels;
	nframes_t max_frames;
	T * buffer;
};

#include "deinterleaver-inl.h"

} // namespace

#endif // AUDIOGRAPHER_DEINTERLEAVER_H
