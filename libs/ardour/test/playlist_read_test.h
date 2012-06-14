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

#include "ardour/types.h"
#include "audio_region_test.h"

class PlaylistReadTest : public AudioRegionTest
{
	CPPUNIT_TEST_SUITE (PlaylistReadTest);
	CPPUNIT_TEST (singleReadTest);
	CPPUNIT_TEST (overlappingReadTest);
	CPPUNIT_TEST (transparentReadTest);
	CPPUNIT_TEST (enclosedTransparentReadTest);
	CPPUNIT_TEST (miscReadTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void tearDown ();
	
	void singleReadTest ();
	void overlappingReadTest ();
	void transparentReadTest ();
	void enclosedTransparentReadTest ();
	void miscReadTest ();

private:
	int _N;
	ARDOUR::Sample* _buf;
	ARDOUR::Sample* _mbuf;
	float* _gbuf;
	
	void check_staircase (ARDOUR::Sample *, int, int);
};
