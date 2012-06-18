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
#include "ardour/region_factory.h"
#include "region_naming_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (RegionNamingTest);

using namespace std;
using namespace ARDOUR;

void
RegionNamingTest::basicsTest ()
{
	for (int i = 0; i < 64; ++i) {
		boost::shared_ptr<Region> r = RegionFactory::create (_r[0], true);
		stringstream s;
		s << "ar0." << (i + 1);
		CPPUNIT_ASSERT_EQUAL (s.str(), r->name());
	}

	_r[0]->set_name ("foo");

	for (int i = 0; i < 64; ++i) {
		boost::shared_ptr<Region> r = RegionFactory::create (_r[0], true);
		stringstream s;
		s << "foo." << (i + 1);
		CPPUNIT_ASSERT_EQUAL (s.str(), r->name());
	}

	for (int i = 0; i < 64; ++i) {
		boost::shared_ptr<Region> rA = RegionFactory::create (_r[0], true);
		boost::shared_ptr<Region> rB = RegionFactory::create (rA, true);
		stringstream s;
		s << "foo." << (i * 2 + 64 + 1);
		CPPUNIT_ASSERT_EQUAL (s.str(), rA->name());
		stringstream t;
		t << "foo." << (i * 2 + 64 + 2);
		CPPUNIT_ASSERT_EQUAL (s.str(), rA->name());
	}
}

void
RegionNamingTest::cacheTest ()
{
	/* Check that all the regions in the map are on the name list */

	CPPUNIT_ASSERT_EQUAL (RegionFactory::region_map.size(), RegionFactory::region_name_map.size());

	for (RegionFactory::RegionMap::iterator i = RegionFactory::region_map.begin(); i != RegionFactory::region_map.end(); ++i) {
		CPPUNIT_ASSERT (RegionFactory::region_name_map.find (i->second->name()) != RegionFactory::region_name_map.end ());
	}
}
