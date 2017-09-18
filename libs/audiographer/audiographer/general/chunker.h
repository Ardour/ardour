#ifndef AUDIOGRAPHER_CHUNKER_H
#define AUDIOGRAPHER_CHUNKER_H

#include "audiographer/visibility.h"
#include "audiographer/flag_debuggable.h"
#include "audiographer/sink.h"
#include "audiographer/type_utils.h"
#include "audiographer/utils/listed_source.h"

namespace AudioGrapher
{

/// A class that chunks process cycles into equal sized samples
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
	Chunker (samplecnt_t chunk_size)
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

		samplecnt_t samples_left = context.samples();
		samplecnt_t input_position = 0;

		while (position + samples_left >= chunk_size) {
			// Copy from context to buffer
			samplecnt_t const samples_to_copy = chunk_size - position;
			TypeUtils<T>::copy (&context.data()[input_position], &buffer[position], samples_to_copy);

			// Update counters
			position = 0;
			input_position += samples_to_copy;
			samples_left -= samples_to_copy;

			// Output whole buffer
			ProcessContext<T> c_out (context, buffer, chunk_size);
			if (samples_left) { c_out.remove_flag(ProcessContext<T>::EndOfInput); }
			ListedSource<T>::output (c_out);
		}

		if (samples_left) {
			// Copy the rest of the data
			TypeUtils<T>::copy (&context.data()[input_position], &buffer[position], samples_left);
			position += samples_left;
		}

		if (context.has_flag (ProcessContext<T>::EndOfInput) && position > 0) {
			ProcessContext<T> c_out (context, buffer, position);
			ListedSource<T>::output (c_out);
		}
	}
	using Sink<T>::process;

  private:
	samplecnt_t chunk_size;
	samplecnt_t position;
	T * buffer;

};

} // namespace

#endif // AUDIOGRAPHER_CHUNKER_H

