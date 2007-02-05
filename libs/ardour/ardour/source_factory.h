#ifndef __ardour_source_factory_h__
#define __ardour_source_factory_h__

#include <string>
#include <stdint.h>
#include <sigc++/sigc++.h>
#include <boost/shared_ptr.hpp>

#include <ardour/source.h>
#include <ardour/audiofilesource.h>

class XMLNode;

namespace ARDOUR {

class Session;

class SourceFactory {
  public:
	static sigc::signal<void,boost::shared_ptr<Source> > SourceCreated;

	static boost::shared_ptr<Source> create (Session&, const XMLNode& node);
	static boost::shared_ptr<Source> createSilent (Session&, const XMLNode& node, nframes_t nframes, float sample_rate);

	// MIDI sources will have to be hacked in here somehow
	static boost::shared_ptr<Source> createReadable (Session&, std::string path, int chn, AudioFileSource::Flag flags, bool announce = true);
	static boost::shared_ptr<Source> createWritable (Session&, std::string name, bool destructive, nframes_t rate, bool announce = true);

  private:
	static int setup_peakfile (boost::shared_ptr<Source>);
};

}

#endif /* __ardour_source_factory_h__ */
