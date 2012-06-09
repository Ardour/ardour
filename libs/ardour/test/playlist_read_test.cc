#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/session.h"
#include "playlist_read_test.h"
#include "test_globals.h"

CPPUNIT_TEST_SUITE_REGISTRATION (PlaylistReadTest);

using namespace std;
using namespace ARDOUR;

void
PlaylistReadTest::setUp ()
{
	TestNeedingPlaylistAndRegions::setUp ();

	_N = 1024;
	_buf = new Sample[_N];
	_mbuf = new Sample[_N];
	_gbuf = new float[_N];

	_session->config.set_auto_xfade (false);

	_apl = boost::dynamic_pointer_cast<AudioPlaylist> (_playlist);

	for (int i = 0; i < _N; ++i) {
		_buf[i] = 0;
	}
}

void
PlaylistReadTest::tearDown ()
{
	delete[] _buf;
	delete[] _mbuf;
	delete[] _gbuf;

	_apl.reset ();

	TestNeedingPlaylistAndRegions::tearDown ();
}

void
PlaylistReadTest::singleReadTest ()
{
	/* Single-region read with fades */

	boost::shared_ptr<AudioRegion> ar0 = boost::dynamic_pointer_cast<AudioRegion> (_region[0]);
	ar0->set_name ("ar0");
	_apl->add_region (ar0, 0);
	ar0->set_default_fade_in ();
	ar0->set_default_fade_out ();
	CPPUNIT_ASSERT_EQUAL (double (64), ar0->_fade_in->back()->when);
	CPPUNIT_ASSERT_EQUAL (double (64), ar0->_fade_out->back()->when);
	ar0->set_length (1024);
	_apl->read (_buf, _mbuf, _gbuf, 0, 256, 0);
	
	for (int i = 0; i < 64; ++i) {
		/* Note: this specific float casting is necessary so that the rounding
		   is done here the same as it is done in AudioPlaylist.
		*/
		CPPUNIT_ASSERT_DOUBLES_EQUAL (float (i * float (i / 63.0)), _buf[i], 1e-16);
	}
	
	for (int i = 64; i < 256; ++i) {
		CPPUNIT_ASSERT_EQUAL (i, int (_buf[i]));
	}
}

void
PlaylistReadTest::overlappingReadTest ()
{
	/* Overlapping read; ar0 and ar1 are both 1024 frames long, ar0 starts at 0,
	   ar1 starts at 128.  We test a read from 0 to 256, which should consist
	   of the start of ar0, with its fade in, followed by ar1's fade in (mixed with ar0
	   faded out with the inverse gain), and some more of ar1.
	*/

	boost::shared_ptr<AudioRegion> ar0 = boost::dynamic_pointer_cast<AudioRegion> (_region[0]);
	ar0->set_name ("ar0");
	_apl->add_region (ar0, 0);
	ar0->set_default_fade_in ();
	ar0->set_default_fade_out ();
	CPPUNIT_ASSERT_EQUAL (double (64), ar0->_fade_in->back()->when);
	CPPUNIT_ASSERT_EQUAL (double (64), ar0->_fade_out->back()->when);
	ar0->set_length (1024);

	/* Note: these are ordinary fades, not xfades */
	CPPUNIT_ASSERT_EQUAL (false, ar0->fade_in_is_xfade());
	CPPUNIT_ASSERT_EQUAL (false, ar0->fade_out_is_xfade());
	
	boost::shared_ptr<AudioRegion> ar1 = boost::dynamic_pointer_cast<AudioRegion> (_region[1]);
	ar1->set_name ("ar1");
	_apl->add_region (ar1, 128);
	ar1->set_default_fade_in ();
	ar1->set_default_fade_out ();

	/* Note: these are ordinary fades, not xfades */
	CPPUNIT_ASSERT_EQUAL (false, ar1->fade_in_is_xfade());
	CPPUNIT_ASSERT_EQUAL (false, ar1->fade_out_is_xfade());
	
	CPPUNIT_ASSERT_EQUAL (double (64), ar1->_fade_in->back()->when);
	CPPUNIT_ASSERT_EQUAL (double (64), ar1->_fade_out->back()->when);
	
	ar1->set_length (1024);
	_apl->read (_buf, _mbuf, _gbuf, 0, 256, 0);

	/* ar0's fade in */
	for (int i = 0; i < 64; ++i) {
		/* Note: this specific float casting is necessary so that the rounding
		   is done here the same as it is done in AudioPlaylist; the gain factor
		   must be computed using double precision, with the result then cast
		   to float.
		*/
		CPPUNIT_ASSERT_DOUBLES_EQUAL (float (i * float (i / (double) 63)), _buf[i], 1e-16);
	}

	/* bit of ar0 */
	for (int i = 64; i < 128; ++i) {
		CPPUNIT_ASSERT_EQUAL (i, int (_buf[i]));
	}

	/* ar1's fade in with faded-out ar0 */
	for (int i = 0; i < 64; ++i) {
		/* Similar carry-on to above with float rounding */
		float const from_ar0 = (128 + i) * float (1 - (i / (double) 63));
		float const from_ar1 = i * float (i / (double) 63);
		CPPUNIT_ASSERT_DOUBLES_EQUAL (from_ar0 + from_ar1, _buf[i + 128], 1e-16);
	}
}

void
PlaylistReadTest::transparentReadTest ()
{
	boost::shared_ptr<AudioRegion> ar0 = boost::dynamic_pointer_cast<AudioRegion> (_region[0]);
	ar0->set_name ("ar0");
	_apl->add_region (ar0, 0);
	ar0->set_default_fade_in ();
	ar0->set_default_fade_out ();
	CPPUNIT_ASSERT_EQUAL (double (64), ar0->_fade_in->back()->when);
	CPPUNIT_ASSERT_EQUAL (double (64), ar0->_fade_out->back()->when);
	ar0->set_length (1024);
	
	boost::shared_ptr<AudioRegion> ar1 = boost::dynamic_pointer_cast<AudioRegion> (_region[1]);
	ar1->set_name ("ar1");
	_apl->add_region (ar1, 0);
	ar1->set_default_fade_in ();
	ar1->set_default_fade_out ();
	CPPUNIT_ASSERT_EQUAL (double (64), ar1->_fade_in->back()->when);
	CPPUNIT_ASSERT_EQUAL (double (64), ar1->_fade_out->back()->when);
	ar1->set_length (1024);
	ar1->set_opaque (false);

	_apl->read (_buf, _mbuf, _gbuf, 0, 1024, 0);

	/* ar0 and ar1 fade-ins; ar1 is on top, but it is transparent, so
	   its fade in will not affect ar0; ar0 will just fade in by itself,
	   and the two will be mixed.
	*/
	for (int i = 0; i < 64; ++i) {
		float const fade = i / (double) 63;
		float const ar0 = i * fade;
		float const ar1 = i * fade;
		CPPUNIT_ASSERT_DOUBLES_EQUAL (ar0 + ar1, _buf[i], 1e-16);
	}

	/* ar0 and ar1 bodies, mixed */
	for (int i = 64; i < (1024 - 64); ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (float (i * 2), _buf[i], 1e-16);
	}

	/* ar0 and ar1 fade-outs, mixed */
	for (int i = (1024 - 64); i < 1024; ++i) {
		/* Ardour fades out from 1 to VERY_SMALL_SIGNAL, which is 0.0000001,
		   so this fade out expression is a little long-winded.
		*/
		float const fade = (((double) 1 - 0.0000001) / 63) * (1023 - i) + 0.0000001;
		float const ar0 = i * fade;
		float const ar1 = i * fade;
		CPPUNIT_ASSERT_DOUBLES_EQUAL (ar0 + ar1, _buf[i], 1e-16);
	}
}

/* A few tests just to check that nothing nasty is happening with
   memory corruption, really (for running with valgrind).
*/
void
PlaylistReadTest::miscReadTest ()
{
	boost::shared_ptr<AudioRegion> ar0 = boost::dynamic_pointer_cast<AudioRegion> (_region[0]);
	ar0->set_name ("ar0");
	_apl->add_region (ar0, 0);
	ar0->set_default_fade_in ();
	ar0->set_default_fade_out ();
	CPPUNIT_ASSERT_EQUAL (double (64), ar0->_fade_in->back()->when);
	CPPUNIT_ASSERT_EQUAL (double (64), ar0->_fade_out->back()->when);
	ar0->set_length (128);

	/* Read for just longer than the region */
	_apl->read (_buf, _mbuf, _gbuf, 0, 129, 0);

	/* Read for much longer than the region */
	_apl->read (_buf, _mbuf, _gbuf, 0, 1024, 0);

	/* Read one sample */
	_apl->read (_buf, _mbuf, _gbuf, 53, 54, 0);
}

void
PlaylistReadTest::check_staircase (Sample* b, int offset, int N)
{
	for (int i = 0; i < N; ++i) {
		int const j = i + offset;
		CPPUNIT_ASSERT_EQUAL (j, int (b[i]));
	}
}

/* Check the case where we have
 *    |----------- Region A (transparent) ------------------|
 *                     |---- Region B (opaque) --|
 *
 * The result should be a mix of the two during region B's time.
 */

void
PlaylistReadTest::enclosedTransparentReadTest ()
{
	boost::shared_ptr<AudioRegion> ar0 = boost::dynamic_pointer_cast<AudioRegion> (_region[0]);
	ar0->set_name ("ar0");
	_apl->add_region (ar0, 256);
	/* These calls will result in a 64-sample fade */
	ar0->set_fade_in_length (0);
	ar0->set_fade_out_length (0);
	ar0->set_length (256);
	
	boost::shared_ptr<AudioRegion> ar1 = boost::dynamic_pointer_cast<AudioRegion> (_region[1]);
	ar1->set_name ("ar1");
	_apl->add_region (ar1, 0);
	/* These calls will result in a 64-sample fade */
	ar1->set_fade_in_length (0);
	ar1->set_fade_out_length (0);
	ar1->set_length (1024);
	ar1->set_opaque (false);

	_apl->read (_buf, _mbuf, _gbuf, 0, 1024, 0);

	/* First 64 samples should just be ar1, faded in */
	for (int i = 0; i < 64; ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (float (i * float (i / 63.0)), _buf[i], 1e-16);
	}

	/* Then some of ar1 with no fade */
	for (int i = 64; i < 256; ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i, _buf[i], 1e-16);
	}

	/* Then ar1 + ar0 (faded in) for 64 samples */
	for (int i = 256; i < (256 + 64); ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i + float ((i - 256) * float ((i - 256) / 63.0)), _buf[i], 1e-16);
	}

	/* Then ar1 + ar0 for 128 samples */
	for (int i = (256 + 64); i < (256 + 64 + 128); ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i + i - (256 + 64) + 64, _buf[i], 1e-16);
	}
	
	/* Then ar1 + ar0 (faded out) for 64 samples */
	for (int i = (256 + 64 + 128); i < 512; ++i) {
		float const ar0_without_fade = i - 256;
		/* See above regarding VERY_SMALL_SIGNAL SNAFU */
		float const fade = (((double) 1 - 0.0000001) / 63) * (511 - i) + 0.0000001;
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i + float (ar0_without_fade * fade), _buf[i], 1e-16);
	}

	/* Then just ar1 for a while */
	for (int i = 512; i < (1024 - 64); ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i, _buf[i], 1e-16);
	}

	/* And finally ar1's fade out */
	for (int i = (1024 - 64); i < 1024; ++i) {
		/* See above regarding VERY_SMALL_SIGNAL SNAFU */
		float const fade = (((double) 1 - 0.0000001) / 63) * (1023 - i) + 0.0000001;
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i * fade, _buf[i], 1e-16);

	}
}
