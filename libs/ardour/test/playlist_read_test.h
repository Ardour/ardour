#include "ardour/types.h"
#include "test_needing_playlist_and_regions.h"

class PlaylistReadTest : public TestNeedingPlaylistAndRegions
{
	CPPUNIT_TEST_SUITE (PlaylistReadTest);
	CPPUNIT_TEST (singleReadTest);
	CPPUNIT_TEST (overlappingReadTest);
	CPPUNIT_TEST (transparentReadTest);
	CPPUNIT_TEST (miscReadTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void tearDown ();
	
	void singleReadTest ();
	void overlappingReadTest ();
	void transparentReadTest ();
	void miscReadTest ();

private:
	int _N;
	ARDOUR::Sample* _buf;
	ARDOUR::Sample* _mbuf;
	float* _gbuf;
	boost::shared_ptr<ARDOUR::AudioPlaylist> _apl;
	
	void check_staircase (ARDOUR::Sample *, int, int);
};
