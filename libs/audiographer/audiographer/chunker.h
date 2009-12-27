#ifndef AUDIOGRAPHER_CHUNKER_H
#define AUDIOGRAPHER_CHUNKER_H

#include "listed_source.h"
#include "sink.h"
#include <cstring>

namespace AudioGrapher
{

template<typename T>
class Chunker : public ListedSource<T>, public Sink<T>
{
  public:
	Chunker (nframes_t chunk_size)
	  : chunk_size (chunk_size)
	  , position (0)
	{
		buffer = new T[chunk_size];
	}
	
	~Chunker()
	{
		delete [] buffer;
	}
	
	void process (ProcessContext<T> const & context)
	{
		if (position + context.frames() < chunk_size) {
			memcpy (&buffer[position], (float const *)context.data(), context.frames() * sizeof(T));
			position += context.frames();
		} else {
			nframes_t const frames_to_copy = chunk_size - position;
			memcpy (&buffer[position], context.data(), frames_to_copy * sizeof(T));
			ProcessContext<T> c_out (context, buffer, chunk_size);
			ListedSource<T>::output (c_out);
			
			memcpy (buffer, &context.data()[frames_to_copy], (context.frames() - frames_to_copy) * sizeof(T));
			position =  context.frames() - frames_to_copy;
		}
	}
	using Sink<T>::process;
	
  private:
	nframes_t chunk_size;
	nframes_t position;
	T * buffer;
	
};

} // namespace

#endif // AUDIOGRAPHER_CHUNKER_H

