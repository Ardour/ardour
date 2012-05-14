#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/audioregion.h"
#include "audio_region_test.h"
#include "test_globals.h"

CPPUNIT_TEST_SUITE_REGISTRATION (AudioRegionTest);

using namespace std;
using namespace ARDOUR;

void
AudioRegionTest::readTest ()
{
	int const N = 1024;
	
	Sample buf[N];
	Sample mbuf[N];
	float gbuf[N];

	int const P = 100;
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (_region[0]);

	/* Simple read: 256 frames from start of region, no fades */

	ar->set_position (P);
	ar->set_length (1024);

	ar->read_from_sources (ar->_sources, ar->_length, buf, P, 256, 0);
	check_staircase (buf, 0, 256);

	for (int i = 0; i < N; ++i) {
		buf[i] = 0;
	}

	/* Offset read: 256 frames from 128 frames into the region, no fades */
	ar->read_from_sources (ar->_sources, ar->_length, buf, P + 128, 256, 0);
	check_staircase (buf, 128, 256);

	/* Simple read with a fade-in: 256 frames from start of region, with fades */
	ar->set_default_fade_in ();
	CPPUNIT_ASSERT_EQUAL (double (64), ar->_fade_in->back()->when);

	for (int i = 0; i < N; ++i) {
		buf[i] = 0;
	}

	ar->read_at (buf, mbuf, gbuf, P, 256, 0);
	for (int i = 0; i < 64; ++i) {
		/* XXX: this isn't very accurate, but close enough for now; needs investigation */
		CPPUNIT_ASSERT_DOUBLES_EQUAL (float (i * i / 63.0), buf[i], 1e-4);
	}
	for (int i = 64; i < P; ++i) {
		CPPUNIT_ASSERT_EQUAL (i, int (buf[i]));
	}
	
	/* Offset read: 256 frames from 128 frames into the region, with fades
	   (though the fade should not affect it, as it is finished before the read starts)
	*/

	for (int i = 0; i < N; ++i) {
		buf[i] = 0;
	}
	
	ar->read_at (buf, mbuf, gbuf, P + 128, 256, 0);
	check_staircase (buf, 128, 256);
}

void
AudioRegionTest::check_staircase (Sample* b, int offset, int N)
{
	for (int i = 0; i < N; ++i) {
		int const j = i + offset;
		CPPUNIT_ASSERT_EQUAL (j, int (b[i]));
	}
}
