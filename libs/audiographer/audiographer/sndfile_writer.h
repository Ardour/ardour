#ifndef AUDIOGRAPHER_SNDFILE_WRITER_H
#define AUDIOGRAPHER_SNDFILE_WRITER_H

#include "sndfile_base.h"
#include "types.h"
#include "sink.h"

namespace AudioGrapher
{

/// Template parameter specific parts of sndfile writer
template <typename T>
class SndfileWriter : public virtual SndfileBase, public Sink<T>
{
  public:
	SndfileWriter (ChannelCount channels, nframes_t samplerate, int format, std::string const & path);
	
	void process (ProcessContext<T> const & c);
	using Sink<T>::process;

  private:

	void init (); // Inits write function
	sf_count_t (*write_func)(SNDFILE *, const T *, sf_count_t);
};

} // namespace

#endif // AUDIOGRAPHER_SNDFILE_WRITER_H