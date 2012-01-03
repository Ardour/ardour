#include "ardour/playlist.h"
#include "ardour/region.h"
#include "playlist_layering_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (PlaylistLayeringTest);

using namespace std;
using namespace ARDOUR;

void
PlaylistLayeringTest::basicsTest ()
{
	_playlist->add_region (_region[0], 0);
	_playlist->add_region (_region[1], 10);
	_playlist->add_region (_region[2], 20);

	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[2]->layer ());

	_region[0]->set_position (5);

	/* region move should have no effect */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[2]->layer ());
}
