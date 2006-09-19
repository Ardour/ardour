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

	// MIDI sources will have to be hacked in here somehow
	static boost::shared_ptr<Source> createReadable (DataType type, Session&, std::string idstr, AudioFileSource::Flag flags, bool announce = true);
	static boost::shared_ptr<Source> createWritable (DataType type, Session&, std::string name, bool destructive, jack_nframes_t rate, bool announce = true);
};

}

#endif /* __ardour_source_factory_h__ */
