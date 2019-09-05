/*
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cassert>
#include <stdint.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "ardour/tempo.h"
#include "ardour/transport_master.h"

#include "test_needing_session.h"

namespace ARDOUR {

class MclkTestMaster : public ARDOUR::MIDIClock_TransportMaster
{

public:
	MclkTestMaster () : MIDIClock_TransportMaster ("MClk-test", 24) {}
	void testStepResponse ();
};

class MIDIClock_Test : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE(MIDIClock_Test);
	CPPUNIT_TEST(run_test);
	CPPUNIT_TEST_SUITE_END();

public:
	void run_test ();
};

} // namespace ARDOUR
