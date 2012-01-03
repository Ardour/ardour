#include "test_needing_playlist_and_regions.h"

class PlaylistLayeringTest : public TestNeedingPlaylistAndRegions
{
	CPPUNIT_TEST_SUITE (PlaylistLayeringTest);
	CPPUNIT_TEST (basicsTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void basicsTest ();
};
