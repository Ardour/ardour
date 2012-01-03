#include "test_needing_session.h"

namespace ARDOUR {
	class Playlist;
	class Source;
}

class PlaylistLayeringTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (PlaylistLayeringTest);
	CPPUNIT_TEST (basicsTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void tearDown ();

	void basicsTest ();

private:
	void create_three_short_regions ();
	
	boost::shared_ptr<ARDOUR::Playlist> _playlist;
	boost::shared_ptr<ARDOUR::Source> _source;
	boost::shared_ptr<ARDOUR::Region> _region[16];
};
