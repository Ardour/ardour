#include "test_needing_playlist_and_regions.h"

class CombineRegionsTest : public TestNeedingPlaylistAndRegions
{
	CPPUNIT_TEST_SUITE (CombineRegionsTest);
	CPPUNIT_TEST (crossfadeTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void crossfadeTest ();

private:
	void check_crossfade ();
};
