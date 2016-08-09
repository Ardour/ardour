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

	/* no need to supply the frame for a new music-locked meter */
	map.add_meter (meter, 4.0, BBT_Time(2, 1, 0), 0, MusicTime);

	/* add some good stuff here */
}

void
BBTTest::subtractTest ()
{
}
