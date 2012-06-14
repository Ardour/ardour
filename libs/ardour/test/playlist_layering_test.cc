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
#include "ardour/region.h"
#include "playlist_layering_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (PlaylistLayeringTest);

using namespace std;
using namespace ARDOUR;

void
PlaylistLayeringTest::basicsTest ()
{
	_playlist->add_region (_r[0], 0);
	_playlist->add_region (_r[1], 10);
	_playlist->add_region (_r[2], 20);

	CPPUNIT_ASSERT_EQUAL (layer_t (0), _r[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _r[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _r[2]->layer ());

	_r[0]->set_position (5);

	/* region move should have no effect */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _r[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _r[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _r[2]->layer ());
}
