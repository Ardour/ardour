#ifndef AUDIOGRAPHER_TMP_FILE_H
#define AUDIOGRAPHER_TMP_FILE_H

#include "sndfile_writer.h"
#include "sndfile_reader.h"

namespace AudioGrapher
{

template<typename T>
class TmpFile : public SndfileWriter<T>, public SndfileReader<T>
{
  public:
	  
	TmpFile (ChannelCount channels, nframes_t samplerate, int format)
	  : SndfileBase      (channels, samplerate, format, "temp")
	  , SndfileWriter<T> (channels, samplerate, format, "temp")
	  , SndfileReader<T> (channels, samplerate, format, "temp")
	{}
	
};

} // namespace

#endif // AUDIOGRAPHER_TMP_FILE_H