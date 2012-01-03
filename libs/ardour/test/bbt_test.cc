#include <cassert>
#include "ardour/tempo.h"
#include "bbt_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(BBTTest);

using namespace std;
using namespace ARDOUR;

using Timecode::BBT_Time;

void
BBTTest::addTest ()
{
	TempoMap map(48000);
	Tempo    tempo(120);
	Meter    meter(4.0, 4.0);

	map.add_meter (meter, BBT_Time(1, 1, 0));
	
	/* add some good stuff here */
}

void
BBTTest::subtractTest ()
{
}
