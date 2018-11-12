#ifndef AUDIOGRAPHER_TMP_FILE_SYNC_H
#define AUDIOGRAPHER_TMP_FILE_SYNC_H

#include <cstdio>
#include <string>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include "sndfile_writer.h"
#include "sndfile_reader.h"
#include "tmp_file.h"

namespace AudioGrapher
{

/// A temporary file deleted after this class is destructed
template<typename T = DefaultSampleType>
class TmpFileSync
	: public TmpFile<T>
{
  public:

	/// \a filename_template must match the requirements for mkstemp, i.e. end in "XXXXXX"
	TmpFileSync (char * filename_template, int format, ChannelCount channels, samplecnt_t samplerate)
		: SndfileHandle (g_mkstemp(filename_template), true, SndfileBase::ReadWrite, format, channels, samplerate)
		, filename (filename_template)
	{}

	TmpFileSync (int format, ChannelCount channels, samplecnt_t samplerate)
	  : SndfileHandle (fileno (tmpfile()), true, SndfileBase::ReadWrite, format, channels, samplerate)
	{}

	TmpFileSync (TmpFileSync const & other) : SndfileHandle (other) {}
	using SndfileHandle::operator=;

	~TmpFileSync()
	{
		/* explicitly close first, some OS (yes I'm looking at you windows)
		 * cannot delete files that are still open
		 */
		if (!filename.empty()) {
			SndfileBase::close();
			std::remove(filename.c_str());
		}
	}

	void process (ProcessContext<T> const & c)
	{
		SndfileWriter<T>::process (c);

		if (c.has_flag(ProcessContext<T>::EndOfInput)) {
			TmpFile<T>::FileFlushed ();
		}
	}

	using Sink<T>::process;

  private:
	std::string filename;
};

} // namespace

#endif // AUDIOGRAPHER_TMP_FILE_SYNC_H
