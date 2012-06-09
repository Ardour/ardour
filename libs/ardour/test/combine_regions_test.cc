#include "combine_regions_test.h"
#include "ardour/types.h"
#include "ardour/audioplaylist.h"
#include "ardour/region.h"
#include "ardour/audioregion.h"
#include "evoral/Curve.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION (CombineRegionsTest);

using namespace std;
using namespace ARDOUR;

void
CombineRegionsTest::check_crossfade ()
{
	ARDOUR::Sample buf[512];
	ARDOUR::Sample mbuf[512];
	float gbuf[512];

	boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist> (_playlist);

	/* Read from the playlist */
	apl->read (buf, mbuf, gbuf, 0, 256 * 2 - 128, 0);

	/* region[0]'s fade in */
	for (int i = 0; i < 64; ++i) {
		float const fade = i / (double) 63;
		float const r0 = i * fade;
		CPPUNIT_ASSERT_DOUBLES_EQUAL (r0, buf[i], 1e-16);
	}

	/* Some more of region[0] */
	for (int i = 64; i < 128; ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i, buf[i], 1e-16);
	}

	boost::shared_ptr<AudioRegion> ar0 = boost::dynamic_pointer_cast<AudioRegion> (_region[0]);
	boost::shared_ptr<AudioRegion> ar1 = boost::dynamic_pointer_cast<AudioRegion> (_region[1]);
	
	float fade_in[128];
	float fade_out[128];
	
	ar1->fade_in()->curve().get_vector (0, 128, fade_in, 128);
	ar1->inverse_fade_in()->curve().get_vector (0, 128, fade_out, 128);
	
	/* Crossfading region[0] to region[1] using region[1]'s fade in and inverse fade in.
	   region[0] also has a standard region fade out to add to the fun.
	*/
	for (int i = 128; i < 256; ++i) {
		
		float region_fade_out = 1;
		if (i >= 192) {
			/* Ardour fades out from 1 to VERY_SMALL_SIGNAL, which is 0.0000001,
			   so this fade out expression is a little long-winded.
			*/
			region_fade_out = (((double) 1 - 0.0000001) / 63) * (255 - i) + 0.0000001;
		}

		/* This computation of r0 cannot be compressed into one line, or there
		   is a small floating point `error'
		*/
		float r0 = i * region_fade_out;
		r0 *= fade_out[i - 128];
		
		float const r1 = (i - 128) * fade_in[i - 128];
		CPPUNIT_ASSERT_DOUBLES_EQUAL (r0 + r1, buf[i], 1e-16);
	}

	/* Rest of region[1] */
	for (int i = 256; i < (384 - 64); ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i - 128, buf[i], 1e-16);
	}

	/* And region[1]'s fade out */
	for (int i = (384 - 64); i < 384; ++i) {
		float const fade_out = (((double) 1 - 0.0000001) / 63) * (383 - i) + 0.0000001;
		CPPUNIT_ASSERT_DOUBLES_EQUAL ((i - 128) * fade_out, buf[i], 1e-16);
	}
}

void
CombineRegionsTest::crossfadeTest ()
{
	/* Two regions, both 256 frames in length, overlapping by 128 frames in the middle */

	boost::shared_ptr<AudioRegion> ar0 = boost::dynamic_pointer_cast<AudioRegion> (_region[0]);
	ar0->set_name ("ar0");
	boost::shared_ptr<AudioRegion> ar1 = boost::dynamic_pointer_cast<AudioRegion> (_region[1]);
	ar1->set_name ("ar1");

	ar0->set_default_fade_in ();
	ar0->set_default_fade_out ();
	ar1->set_default_fade_out ();
	
	_playlist->add_region (_region[0], 0);
	_region[0]->set_length (256);

	_playlist->add_region (_region[1], 128);
	_region[1]->set_length (256);

	/* Check that the right fades have been set up */

	CPPUNIT_ASSERT_EQUAL (false, ar0->fade_in_is_xfade ());
	CPPUNIT_ASSERT_EQUAL (false, ar0->fade_out_is_xfade ());
	CPPUNIT_ASSERT_EQUAL (true, ar1->fade_in_is_xfade ());
	CPPUNIT_ASSERT_EQUAL (false, ar1->fade_out_is_xfade ());

	/* Check that the read comes back correctly */
	
	check_crossfade ();

	/* Combine the two regions */

	RegionList rl;
	rl.push_back (_region[0]);
	rl.push_back (_region[1]);
	_playlist->combine (rl);

	/* ...so we just have the one region... */
	CPPUNIT_ASSERT_EQUAL ((uint32_t) 1, _playlist->n_regions ());

	/* And reading should give the same thing */

	check_crossfade ();
}

