#ifndef AUDIOGRAPHER_SNDFILE_BASE_H
#define AUDIOGRAPHER_SNDFILE_BASE_H

#include <string>
#include <sndfile.h>
#include <sigc++/signal.h>

#include "types.h"
#include "debuggable.h"

namespace AudioGrapher {

/// Common interface for templated libsndfile readers/writers
class SndfileBase : public Debuggable<>
{
  public:
	
	sigc::signal<void, std::string> FileWritten;

  protected:
	SndfileBase (ChannelCount channels, nframes_t samplerate, int format, std::string const & path);
	virtual ~SndfileBase ();

	std::string    path;
	SF_INFO        sf_info;
	SNDFILE *      sndfile;
};

} // namespace

#endif // AUDIOGRAPHER_SNDFILE_BASE_H