#include <stdlib.h>

#include "temporal/tempo.h"

#include "TempoMapTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TempoMapTest);

using namespace Temporal;

void
TempoMapTest::createTest()
{
	CPPUNIT_ASSERT (TempoMap::use() != 0);
}

void
TempoMapTest::addTest()
{
	TempoMap::WritableSharedPtr tmap (TempoMap::write_copy());
	TempoPoint& tp = tmap->set_tempo (Tempo (180, 4), BBT_Time (6, 1, 0));
	tmap->set_meter (Meter (6, 8), BBT_Time (3, 1, 0));

	/* tp is at 6|1|0 which is 3|0|0 after the 6/8 meter at ((3-1) * 4 =) 8
	 * quarter notes), so 3 bars of 6
	 * 8th notes, or 18 8 notes, or 9 4th notes. So its quarter time should
	 * be 8 + 9 = 17
	 */

	std::cout << "\n\n\n\***************** tp = " << tp.beats() << std::endl;

	CPPUNIT_ASSERT (tp.beats() == Beats (17,0));


	tmap->abort_update ();
}

void
TempoMapTest::subtractTest()
{
}

void
TempoMapTest::multiplyTest()
{
}

void
TempoMapTest::roundTest()
{
}

void
TempoMapTest::convertTest()
{
}

