/*
    Copyright (C) 2012 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

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
CombineRegionsTest::check_crossfade1 ()
{
	ARDOUR::Sample buf[512];
	ARDOUR::Sample mbuf[512];
	float gbuf[512];

	/* Read from the playlist */
	_audio_playlist->read (buf, mbuf, gbuf, 0, 256 * 2 - 128, 0);

	/* _r[0]'s fade in */
	for (int i = 0; i < 64; ++i) {
		float const fade = i / (double) 63;
		float const r0 = i * fade;
		CPPUNIT_ASSERT_DOUBLES_EQUAL (r0, buf[i], 1e-16);
	}

	/* Some more of _r[0] */
	for (int i = 64; i < 128; ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i, buf[i], 1e-16);
	}

	float fade_in[128];
	float fade_out[128];

	_ar[1]->fade_in()->curve().get_vector (0, 128, fade_in, 128);
	_ar[1]->inverse_fade_in()->curve().get_vector (0, 128, fade_out, 128);
	
	/* Crossfading _r[0] to _r[1] using _r[1]'s fade in and inverse fade in.
	   _r[0] also has a standard region fade out to add to the fun.
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

	/* Rest of _r[1] */
	for (int i = 256; i < (384 - 64); ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i - 128, buf[i], 1e-16);
	}

	/* And _r[1]'s fade out */
	for (int i = (384 - 64); i < 384; ++i) {
		float const fade_out = (((double) 1 - 0.0000001) / 63) * (383 - i) + 0.0000001;
		CPPUNIT_ASSERT_DOUBLES_EQUAL ((i - 128) * fade_out, buf[i], 1e-16);
	}
}

/** Test combining two cross-faded regions, with the earlier region
 *  on the lower layer.
 */
void
CombineRegionsTest::crossfadeTest1 ()
{
	/* Two regions, both 256 frames in length, overlapping by 128 frames in the middle */

	_ar[0]->set_default_fade_in ();
	_ar[0]->set_default_fade_out ();
	_ar[1]->set_default_fade_out ();
	
	_playlist->add_region (_r[0], 0);
	_r[0]->set_length (256);

	_playlist->add_region (_r[1], 128);
	_r[1]->set_length (256);

	/* Check layering */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _r[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _r[1]->layer ());

	/* Check that the right fades have been set up */
	CPPUNIT_ASSERT_EQUAL (false, _ar[0]->fade_in_is_xfade ());
	CPPUNIT_ASSERT_EQUAL (false, _ar[0]->fade_out_is_xfade ());
	CPPUNIT_ASSERT_EQUAL (true, _ar[1]->fade_in_is_xfade ());
	CPPUNIT_ASSERT_EQUAL (false, _ar[1]->fade_out_is_xfade ());

	/* Check that the read comes back correctly */
	check_crossfade1 ();

	/* Combine the two regions */

	RegionList rl;
	rl.push_back (_r[0]);
	rl.push_back (_r[1]);
	_playlist->combine (rl);

	/* ...so we just have the one region... */
	CPPUNIT_ASSERT_EQUAL ((uint32_t) 1, _playlist->n_regions ());

	/* And reading should give the same thing */
	check_crossfade1 ();
}

void
CombineRegionsTest::check_crossfade2 ()
{
	ARDOUR::Sample buf[512];
	ARDOUR::Sample mbuf[512];
	float gbuf[512];

	/* Read from the playlist */
	_audio_playlist->read (buf, mbuf, gbuf, 0, 256 * 2 - 128, 0);

	/* _r[0]'s fade in */
	for (int i = 0; i < 64; ++i) {
		float const fade = i / (double) 63;
		float const r0 = i * fade;
		CPPUNIT_ASSERT_DOUBLES_EQUAL (r0, buf[i], 1e-16);
	}

	/* Some more of _r[0] */
	for (int i = 64; i < 128; ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i, buf[i], 1e-16);
	}

	float fade_in[128];
	float fade_out[128];

	_ar[0]->inverse_fade_out()->curve().get_vector (0, 128, fade_in, 128);
	_ar[0]->fade_out()->curve().get_vector (0, 128, fade_out, 128);
	
	/* Crossfading _r[0] to _r[1] using _r[0]'s fade out and inverse fade out.
	   _r[1] also has a standard region fade in to add to the fun.
	*/
	for (int i = 128; i < 256; ++i) {

		float region_fade_in = 1;
		if (i < (128 + 64)) {
			region_fade_in = (i - 128) / ((double) 63);
		}

		float r0 = i * fade_out[i - 128];
		float r1 = (i - 128) * region_fade_in;
		r1 *= fade_in[i - 128];
		
		CPPUNIT_ASSERT_DOUBLES_EQUAL (r0 + r1, buf[i], 1e-16);
	}

	/* Rest of _r[1] */
	for (int i = 256; i < (384 - 64); ++i) {
		CPPUNIT_ASSERT_DOUBLES_EQUAL (i - 128, buf[i], 1e-16);
	}

	/* And _r[1]'s fade out */
	for (int i = (384 - 64); i < 384; ++i) {
		float const fade_out = (((double) 1 - 0.0000001) / 63) * (383 - i) + 0.0000001;
		CPPUNIT_ASSERT_DOUBLES_EQUAL ((i - 128) * fade_out, buf[i], 1e-16);
	}
}

/** As per crossfadeTest1, except that the earlier region is on the
 *  higher layer.
 */
void
CombineRegionsTest::crossfadeTest2 ()
{
	/* Two regions, both 256 frames in length, overlapping by 128 frames in the middle */

	_ar[0]->set_default_fade_in ();
	_ar[0]->set_default_fade_out ();
	_ar[1]->set_default_fade_out ();
	
	_playlist->add_region (_r[0], 0);
	_r[0]->set_length (256);

	_playlist->add_region (_r[1], 128);
	_r[1]->set_length (256);

	_r[1]->lower_to_bottom ();

	/* Check layering */
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _r[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _r[1]->layer ());

	/* Check that the right fades have been set up */

	CPPUNIT_ASSERT_EQUAL (false, _ar[0]->fade_in_is_xfade ());
	CPPUNIT_ASSERT_EQUAL (true, _ar[0]->fade_out_is_xfade ());
	CPPUNIT_ASSERT_EQUAL (false, _ar[1]->fade_in_is_xfade ());
	CPPUNIT_ASSERT_EQUAL (false, _ar[1]->fade_out_is_xfade ());

	/* Check that the read comes back correctly */
	check_crossfade2 ();

	/* Combine the two regions */

	RegionList rl;
	rl.push_back (_r[0]);
	rl.push_back (_r[1]);
	_playlist->combine (rl);

	/* ...so we just have the one region... */
	CPPUNIT_ASSERT_EQUAL ((uint32_t) 1, _playlist->n_regions ());

	/* And reading should give the same thing */
	check_crossfade2 ();
}

