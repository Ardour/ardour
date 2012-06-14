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

#include "audio_region_test.h"

namespace ARDOUR {
	class AutomationList;
}

class CombineRegionsTest : public AudioRegionTest
{
	CPPUNIT_TEST_SUITE (CombineRegionsTest);
	CPPUNIT_TEST (crossfadeTest1);
	CPPUNIT_TEST (crossfadeTest2);
	CPPUNIT_TEST_SUITE_END ();

public:
	void crossfadeTest1 ();
	void crossfadeTest2 ();

private:
	void check_crossfade1 ();
	void check_crossfade2 ();
};
