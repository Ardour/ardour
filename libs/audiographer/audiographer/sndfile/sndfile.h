#ifndef AUDIOGRAPHER_SNDFILE_H
#define AUDIOGRAPHER_SNDFILE_H

#include "sndfile_writer.h"
#include "sndfile_reader.h"

namespace AudioGrapher
{

/** Reader/Writer for audio files using libsndfile.
  * Only short, int and float are valid template parameters
  */
template<typename T = DefaultSampleType>
class Sndfile : public SndfileWriter<T>, public SndfileReader<T>
{
  public:

	Sndfile (std::string const & filename, SndfileBase::Mode mode = SndfileBase::ReadWrite, int format = 0,
	         ChannelCount channels = 0, samplecnt_t samplerate = 0)
	  : SndfileHandle (filename, mode, format, channels, samplerate)
	{}

	Sndfile (Sndfile const & other) : SndfileHandle (other) {}
	using SndfileHandle::operator=;

};

} // namespace

#endif // AUDIOGRAPHER_SNDFILE_H
