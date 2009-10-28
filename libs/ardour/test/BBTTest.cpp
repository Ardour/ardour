#include <cassert>
#include "ardour/tempo.h"
#include "BBTTest.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION(BBTTest);

using namespace std;
using namespace ARDOUR;

void
BBTTest::addTest ()
{
	TempoMap map(48000);

	// Test basic operations with a flat tempo map
	CPPUNIT_ASSERT(map.bbt_add(BBT_Time(0, 0, 0), BBT_Time(1, 2, 3)) == BBT_Time(1, 2, 3));
	CPPUNIT_ASSERT(map.bbt_add(BBT_Time(1, 2, 3), BBT_Time(0, 0, 0)) == BBT_Time(1, 2, 3));
}

void
BBTTest::subtractTest ()
{
}
