#ifndef AUDIOGRAPHER_TMP_FILE_H
#define AUDIOGRAPHER_TMP_FILE_H

#include <cstdio>
#include <string>

#include <glib.h>
#include <glib/gstdio.h>

#include "sndfile_writer.h"
#include "sndfile_reader.h"

namespace AudioGrapher
{

/// A temporary file deleted after this class is destructed
template<typename T = DefaultSampleType>
class TmpFile : public SndfileWriter<T>, public SndfileReader<T>
{
  public:

	/// \a filename_template must match the requirements for mkstemp, i.e. end in "XXXXXX"
	TmpFile (char * filename_template, int format, ChannelCount channels, framecnt_t samplerate)
		: SndfileHandle (g_mkstemp(filename_template), true, SndfileBase::ReadWrite, format, channels, samplerate)
		, filename (filename_template)
	{}

	TmpFile (int format, ChannelCount channels, framecnt_t samplerate)
	  : SndfileHandle (fileno (tmpfile()), true, SndfileBase::ReadWrite, format, channels, samplerate)
	{}

	TmpFile (TmpFile const & other) : SndfileHandle (other) {}
	using SndfileHandle::operator=;

	~TmpFile()
	{
		if (!filename.empty()) {
			std::remove(filename.c_str());
		}
	}

  private:
	std::string filename;
};

} // namespace

#endif // AUDIOGRAPHER_TMP_FILE_H
