/*
    Copyright (C) 2012 Paul Davis
    Author: Hans Baier

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

    $Id$
*/

#include <cassert>
#include <stdint.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class MidnamTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(MidnamTest);
	CPPUNIT_TEST(protools_patchfile_test);
	CPPUNIT_TEST(yamaha_PSRS900_patchfile_test);
	CPPUNIT_TEST(load_all_midnams_test);
	CPPUNIT_TEST_SUITE_END();

public:
	typedef double Time;

	void setUp() {
	}

	void tearDown() {
	}

	void protools_patchfile_test();
	void yamaha_PSRS900_patchfile_test();
	void load_all_midnams_test();

private:
};

