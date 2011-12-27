#include "test_needing_session.h"

class PlaylistOverlapCacheTest : public TestNeedingSession
{
public:
	CPPUNIT_TEST_SUITE (PlaylistOverlapCacheTest);
	CPPUNIT_TEST (basicTest);
	CPPUNIT_TEST (stressTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void tearDown ();
	
	void basicTest ();
	void stressTest ();

private:
	boost::shared_ptr<ARDOUR::Playlist> _playlist;
	boost::shared_ptr<ARDOUR::Source> _source;
};
