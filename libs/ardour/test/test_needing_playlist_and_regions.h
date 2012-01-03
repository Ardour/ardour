#include <boost/shared_ptr.hpp>
#include "test_needing_session.h"

namespace ARDOUR {
	class Playlist;
	class Source;
	class Region;
}

class TestNeedingPlaylistAndRegions : public TestNeedingSession
{
public:
	virtual void setUp ();
	virtual void tearDown ();

protected:
	boost::shared_ptr<ARDOUR::Playlist> _playlist;
	boost::shared_ptr<ARDOUR::Source> _source;
	boost::shared_ptr<ARDOUR::Region> _region[16];
};
