#include <cassert>
#include "ardour/tempo.h"
#include "bbt_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(BBTTest);

using namespace std;
using namespace ARDOUR;

using Temporal::BBT_Time;

void
BBTTest::addTest ()
{
	TempoMap map(48000);
	Tempo    tempo(120, 4.0);
	Meter    meter(4.0, 4.0);

	/* no need to supply the sample for a new music-locked meter */
	map.add_meter (meter, BBT_Time(2, 1, 0), 0, MusicTime);

	/* add some good stuff here */
}

void
BBTTest::subtractTest ()
{
}
