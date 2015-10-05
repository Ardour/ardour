#ifndef AUDIOGRAPHER_SNDFILE_BASE_H
#define AUDIOGRAPHER_SNDFILE_BASE_H

// We need to use our modified version until
// the fd patch is accepted upstream
#include "private/sndfile.hh"

namespace AudioGrapher
{

/// Base class for all classes using libsndfile
class SndfileBase : public virtual AudioGrapher::SndfileHandle
{
  public:
	enum Mode
	{
		Read = SFM_READ,
		Write = SFM_WRITE,
		ReadWrite = SFM_RDWR
	};

  protected:
	SndfileBase () {}
};

} // namespace

#endif // AUDIOGRAPHER_SNDFILE_BASE_H