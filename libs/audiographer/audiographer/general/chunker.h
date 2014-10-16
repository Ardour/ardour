#ifndef AUDIOGRAPHER_CHUNKER_H
#define AUDIOGRAPHER_CHUNKER_H

#include "audiographer/visibility.h"
#include "audiographer/flag_debuggable.h"
#include "audiographer/sink.h"
#include "audiographer/type_utils.h"
#include "audiographer/utils/listed_source.h"

namespace AudioGrapher
{

/// A class that chunks process cycles into equal sized frames
template<typename T = DefaultSampleType>
class /*LIBAUDIOGRAPHER_API*/ Chunker
  : public ListedSource<T>
  , public Sink<T>
  , public FlagDebuggable<>
{
  public:
	/** Constructs a new Chunker with a constant chunk size.
	  * \n NOT RT safe
	  */
	Chunker (framecnt_t chunk_size)
	  : chunk_size (chunk_size)
	  , position (0)
	{
		buffer = new T[chunk_size];
		add_supported_flag (ProcessContext<T>::EndOfInput);
	}
	
	~Chunker()
	{
		delete [] buffer;
	}
	
	/** Outputs data in \a context in chunks with the size specified in the constructor.
	  * Note that some calls might not produce any output, while others may produce several.
	  * \n RT safe
	  */
	void process (ProcessContext<T> const & context)
	{
		check_flags (*this, context);
		
		framecnt_t frames_left = context.frames();
		framecnt_t input_position = 0;
		
		while (position + frames_left >= chunk_size) {
			// Copy from context to buffer
			framecnt_t const frames_to_copy = chunk_size - position;
			TypeUtils<T>::copy (&context.data()[input_position], &buffer[position], frames_to_copy);
			
			// Update counters
			position = 0;
			input_position += frames_to_copy;
			frames_left -= frames_to_copy;

			// Output whole buffer
			ProcessContext<T> c_out (context, buffer, chunk_size);
			if (frames_left) { c_out.remove_flag(ProcessContext<T>::EndOfInput); }
			ListedSource<T>::output (c_out);
		}
		
		if (frames_left) {
			// Copy the rest of the data
			TypeUtils<T>::copy (&context.data()[input_position], &buffer[position], frames_left);
			position += frames_left;
		}
		
		if (context.has_flag (ProcessContext<T>::EndOfInput)) {
			ProcessContext<T> c_out (context, buffer, position);
			ListedSource<T>::output (c_out);
		}
	}
	using Sink<T>::process;
	
  private:
	framecnt_t chunk_size;
	framecnt_t position;
	T * buffer;
	
};

} // namespace

#endif // AUDIOGRAPHER_CHUNKER_H

