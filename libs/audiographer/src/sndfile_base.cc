#include "audiographer/sndfile_base.h"
#include "audiographer/exception.h"

#include <boost/format.hpp>

namespace AudioGrapher
{

using std::string;
using boost::str;
using boost::format;

/* SndfileWriterBase */

SndfileBase::SndfileBase (ChannelCount channels, nframes_t samplerate, int format, string const & path)
  : path (path)
{
	char errbuf[256];

	sf_info.channels = channels;
	sf_info.samplerate = samplerate;
	sf_info.format = format;

	if (!sf_format_check (&sf_info)) {
		throw Exception (*this, "Invalid format in constructor");
	}

	if (path.length() == 0) {
		throw Exception (*this, "No output file specified");
	}

	/* TODO add checks that the directory path exists, and also
	   check if we are overwriting an existing file...
	*/

	// Open file
	if (path.compare ("temp")) {
		if ((sndfile = sf_open (path.c_str(), SFM_WRITE, &sf_info)) == 0) {
			sf_error_str (0, errbuf, sizeof (errbuf) - 1);
			throw Exception (*this, str (boost::format ("Cannot open output file \"%1%\" (%2%)") % path % errbuf));
		}
	} else {
		FILE * file;
		if (!(file = tmpfile ())) {
			throw Exception (*this, "Cannot open tempfile");
		}
		sndfile = sf_open_fd (fileno(file), SFM_RDWR, &sf_info, true);
	}
}

SndfileBase::~SndfileBase ()
{
	sf_close (sndfile);
}

} // namespace
