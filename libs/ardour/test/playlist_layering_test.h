#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace ARDOUR {
	class Session;
	class Playlist;
	class Source;
}

class PlaylistLayeringTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (PlaylistLayeringTest);
	CPPUNIT_TEST (addHigherTest);
	CPPUNIT_TEST (moveAddHigherTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void tearDown ();

	void addHigherTest ();
	void moveAddHigherTest ();

private:
	void create_three_short_regions ();
	
	ARDOUR::Session* _session;
	boost::shared_ptr<ARDOUR::Playlist> _playlist;
	boost::shared_ptr<ARDOUR::Source> _source;
	boost::shared_ptr<ARDOUR::Region> _region[16];
};
