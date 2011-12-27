#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "test_needing_session.h"

namespace ARDOUR {
	class Session;
	class Playlist;
	class Source;
}

class PlaylistLayeringTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (PlaylistLayeringTest);
	CPPUNIT_TEST (lastLayerOpTest);
	CPPUNIT_TEST (addHigher_relayerOnAll_Test);
	CPPUNIT_TEST (addOrBoundsHigher_relayerOnAll_Test);
	CPPUNIT_TEST (laterHigher_relayerOnAll_Test);
	CPPUNIT_TEST (addOrBoundsHigher_relayerWhenNecessary_Test);
	CPPUNIT_TEST (recursiveRelayerTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void tearDown ();

	void lastLayerOpTest ();
	void addHigher_relayerOnAll_Test ();
	void addOrBoundsHigher_relayerOnAll_Test ();
	void laterHigher_relayerOnAll_Test ();
	void addOrBoundsHigher_relayerWhenNecessary_Test ();
	void recursiveRelayerTest ();

private:
	void create_short_regions ();
	
	static int const num_regions;
	enum {
		A = 0,
		B,
		C,
		D,
		E,
		F
	};
	
	boost::shared_ptr<ARDOUR::Playlist> _playlist;
	boost::shared_ptr<ARDOUR::Source> _source;
	boost::shared_ptr<ARDOUR::Region>* _region;
};
