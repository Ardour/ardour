#ifndef AUDIOGRAPHER_TMP_FILE_H
#define AUDIOGRAPHER_TMP_FILE_H

#include "sndfile_writer.h"
#include "sndfile_reader.h"

namespace AudioGrapher
{

/// A temporary file deleted after this class is destructed
template<typename T = DefaultSampleType>
class TmpFile : public SndfileWriter<T>, public SndfileReader<T>
{
  public:
	
	TmpFile (int format, ChannelCount channels, nframes_t samplerate)
	  : SndfileHandle (fileno (tmpfile()), true, SndfileBase::ReadWrite, format, channels, samplerate)
	{}
	
	TmpFile (TmpFile const & other) : SndfileHandle (other) {}
	using SndfileHandle::operator=;
	
};

} // namespace

#endif // AUDIOGRAPHER_TMP_FILE_H