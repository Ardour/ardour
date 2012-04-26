#include "ardour/types.h"
#include "test_needing_playlist_and_regions.h"

class AudioRegionTest : public TestNeedingPlaylistAndRegions
{
	CPPUNIT_TEST_SUITE (AudioRegionTest);
	CPPUNIT_TEST (readTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void readTest ();

private:
	void check_staircase (ARDOUR::Sample *, int, int);
};
