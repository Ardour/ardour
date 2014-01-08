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
#include "ardour/audioregion.h"
#include "audio_region_read_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (AudioRegionReadTest);

using namespace std;
using namespace ARDOUR;

/** Check some basic reads */
void
AudioRegionReadTest::readTest ()
{
	int const N = 1024;
	
	Sample buf[N];
	Sample mbuf[N];
	float gbuf[N];

	int const P = 100;

	/* Simple read: 256 frames from start of region, no fades */

	_ar[0]->set_position (P);
	_ar[0]->set_length (1024);

	_ar[0]->read_from_sources (_ar[0]->_sources, _ar[0]->_length, buf, P, 256, 0);
	check_staircase (buf, 0, 256);

	for (int i = 0; i < N; ++i) {
		buf[i] = 0;
	}

	/* Offset read: 256 frames from 128 frames into the region, no fades */
	_ar[0]->read_from_sources (_ar[0]->_sources, _ar[0]->_length, buf, P + 128, 256, 0);
	check_staircase (buf, 128, 256);

	/* Simple read with a fade-in: 256 frames from start of region, with fades */
	_ar[0]->set_default_fade_in ();
	CPPUNIT_ASSERT_EQUAL (double (64), _ar[0]->_fade_in->back()->when);

	for (int i = 0; i < N; ++i) {
		buf[i] = 0;
	}

	_ar[0]->read_at (buf, mbuf, gbuf, P, 256, 0);
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
	
	_ar[0]->read_at (buf, mbuf, gbuf, P + 128, 256, 0);
	check_staircase (buf, 128, 256);
}

void
AudioRegionReadTest::check_staircase (Sample* b, int offset, int N)
{
	for (int i = 0; i < N; ++i) {
		int const j = i + offset;
		CPPUNIT_ASSERT_EQUAL (j, int (b[i]));
	}
}
