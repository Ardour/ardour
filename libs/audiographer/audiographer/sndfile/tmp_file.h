#ifndef AUDIOGRAPHER_TMP_FILE_H
#define AUDIOGRAPHER_TMP_FILE_H

#include <cstdio>
#include <string>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include "sndfile_writer.h"
#include "sndfile_reader.h"

namespace AudioGrapher
{

/// A temporary file deleted after this class is destructed
template<typename T = DefaultSampleType>
class TmpFile
	: public SndfileWriter<T>
	, public SndfileReader<T>
{
  public:
	virtual ~TmpFile () {}
	PBD::Signal0<void> FileFlushed;

};

} // namespace

#endif // AUDIOGRAPHER_TMP_FILE_H
