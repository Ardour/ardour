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

#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/region.h"
#include "playlist_equivalent_regions_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (PlaylistEquivalentRegionsTest);

using namespace std;
using namespace ARDOUR;

void
PlaylistEquivalentRegionsTest::setUp ()
{
	AudioRegionTest::setUp ();
	
	_playlist_b = PlaylistFactory::create (DataType::AUDIO, *_session, "testB");
}

void
PlaylistEquivalentRegionsTest::tearDown ()
{
	_playlist_b.reset ();

	AudioRegionTest::tearDown ();
}

/* Test simple equivalency operations */
void
PlaylistEquivalentRegionsTest::basicsTest ()
{
	/* Put _r[0] on _playlist */
	_playlist->add_region (_r[0], 42);

	/* And _r[1] on _playlist_b at the same position */
	_playlist_b->add_region (_r[1], 42);

	/* Look for the equivalents to _r[0] on _playlist_b */
	vector<boost::shared_ptr<Region> > e;
	_playlist_b->get_equivalent_regions (_r[0], e);

	/* That should be _r[1] */
	CPPUNIT_ASSERT_EQUAL (size_t (1), e.size ());
	CPPUNIT_ASSERT_EQUAL (e.front(), _r[1]);

	/* Move _r[1] */
	_r[1]->set_position (66);

	/* Look again for the equivalents to _r[0] on _playlist_b */
	e.clear ();
	_playlist_b->get_equivalent_regions (_r[0], e);

	/* There should be none */
	CPPUNIT_ASSERT (e.empty ());
}

void
PlaylistEquivalentRegionsTest::multiLayerTest ()
{
	_playlist->clear ();
	_playlist_b->clear ();

	/* Put _r[0] and _r[1] at the same position on _playlist so that they overlap */
	_playlist->add_region (_r[0], 42);
	_playlist->add_region (_r[1], 42);

	/* And _r[2], _r[3] similarly on _playlist_b */
	_playlist_b->add_region (_r[2], 42);
	_playlist_b->add_region (_r[3], 42);

	/* Look for equivalents to _r[0] on _playlist_b */
	vector<boost::shared_ptr<Region> > e;
	_playlist_b->get_equivalent_regions (_r[0], e);

	/* That should be _r[2] and _r[3] */
	CPPUNIT_ASSERT_EQUAL (size_t (2), e.size ());
	CPPUNIT_ASSERT ((e.front() == _r[2] && e.back() == _r[3]) || (e.front() == _r[3] && e.back() == _r[2]));
}
