#include <cassert>
#include "ardour/tempo.h"
#include "bbt_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(BBTTest);

using namespace std;
using namespace ARDOUR;

void
BBTTest::addTest ()
{
	TempoMap map(48000);
	Tempo    tempo(120);
	Meter    meter(4.0, 4.0);

	map.add_meter (meter, BBT_Time(1, 1, 0));

	// Test basic operations with a flat tempo map
	BBT_Time time = map.bbt_add(BBT_Time(1, 1, 0), BBT_Time(1, 2, 3));
	//cerr << "result: BBT_Time(" << time.bars << ", " << time.beats << ", " 
	//     << time.ticks << ")" << endl;
	CPPUNIT_ASSERT(time == BBT_Time(2, 3, 0));


	time = map.bbt_add(BBT_Time(1, 2, 3), BBT_Time(2, 2, 3));
	//cerr << "result: BBT_Time(" << time.bars << ", " << time.beats << ", " 
	//     << time.ticks << ")" << endl;
	CPPUNIT_ASSERT(time == BBT_Time(3, 4, 3));
}

void
BBTTest::subtractTest ()
{
}
