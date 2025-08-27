#include <stdlib.h>

#include "temporal/bbt_time.h"
#include "temporal/tempo.h"

#include "BBTTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION(BBTTest);

using namespace Temporal;

void
BBTTest::createTest()
{
	CPPUNIT_ASSERT_THROW_MESSAGE ("- zero-bar BBT_Argument;", BBT_Argument (0, 1, 0), IllegalBBTTimeException);
	CPPUNIT_ASSERT_THROW_MESSAGE ("- zero-beat BBT_Argument;", BBT_Argument (1, 0, 0), IllegalBBTTimeException);
	CPPUNIT_ASSERT_THROW_MESSAGE ("- zero-bar-and-beat BBT_Argument;", BBT_Argument (0, 0, 0), IllegalBBTTimeException);

	/* This test checks that BBT_Argument cannot convert ticks to beats etc., so the explicit 1920 ticks remains
	   that in the constructed BBT_Argument, rather than being converted to an additional beat. This has to be true
	   because BBT_Argument has no clue what the meter is.
	*/
	BBT_Argument a (1, 1, 1920);
	CPPUNIT_ASSERT(BBT_Argument (1,2,0) != a);

	/* by contrast, a zero-distance "walk" from that location should return a canonicalized BBT_Argument, which given a 4/4 meter
	   will be 1|2|0
	*/

	TempoMap::SharedPtr tmap (TempoMap::fetch());
	BBT_Offset b (0, 0, 0);
	BBT_Argument r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,2,0), r);

}

void
BBTTest::addTest()
{
	TempoMap::SharedPtr tmap (TempoMap::fetch());
	BBT_Argument a(1,1,0);
	BBT_Offset b(1,0,0);
	BBT_Argument r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (2,1,0), r);

	b = BBT_Offset (0, 1, 0);
	r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,2,0), r);

	b = BBT_Offset (0, 0, 1);
	r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,1,1), r);

	b = BBT_Offset (0, 0, ticks_per_beat - 1);
	r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,1,ticks_per_beat - 1), r);

	b = BBT_Offset (0, 0, ticks_per_beat);
	r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,2,0), r);

	b = BBT_Offset (0, 0, ticks_per_beat * 2);
	r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,3,0), r);

	/* assumes 4/4 time */

	b = BBT_Offset (0, 4, 0);
	r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (2,1,0), r);

	b = BBT_Offset (1, 0, 0);
	r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (2,1,0), r);

	b = BBT_Offset (0, 0, ticks_per_beat * 4);
	r = tmap->bbt_walk (a, b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (2,1,0), r);

}

void
BBTTest::subtractTest()
{
	TempoMap::SharedPtr tmap (TempoMap::fetch());
	BBT_Argument a (1,1,0);
	BBT_Offset b (1,0,0);
	BBT_Argument r = tmap->bbt_walk (a, -b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (-1,1,0), r);

	b = BBT_Offset (0, 1, 0);
	r = tmap->bbt_walk (a, -b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,-1,0), r); /* Not sure this is actually the correct answer */

	b = BBT_Offset (0, 0, 1);
	r = tmap->bbt_walk (a, -b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,1,-1), r); /* Not sure this is actually the correct answer */

	b = BBT_Offset (0, 0, ticks_per_beat);
	r = tmap->bbt_walk (a, -b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,1,-ticks_per_beat), r); /* Not sure this is actually the correct answer */

	b = BBT_Offset (0, 0, ticks_per_beat + 1);
	r = tmap->bbt_walk (a, -b);
	CPPUNIT_ASSERT_EQUAL(BBT_Argument (1,1,-(ticks_per_beat+1)), r); /* Not sure this is actually the correct answer */
}

void
BBTTest::multiplyTest()
{
}

void
BBTTest::roundTest()
{
}

void
BBTTest::convertTest()
{
}

void
BBTTest::deltaTest ()
{
	Meter m (12, 8);
	BBT_Time a (5,12,0);
	BBT_Time b (5,12,480);
	BBT_Offset o (m.bbt_delta (b, a));

	CPPUNIT_ASSERT_EQUAL (BBT_Offset (0, 0, 480), o);

	a = BBT_Time (5,11,959);
	o = m.bbt_delta (b, a);

	CPPUNIT_ASSERT_EQUAL (BBT_Offset (0, 0, 481), o);

	a = BBT_Time (4,11,959);
	o = m.bbt_delta (b, a);

	CPPUNIT_ASSERT_EQUAL (BBT_Offset (1, 0, 481), o);

	a = BBT_Time (4,6,959);
	o = m.bbt_delta (b, a);

	CPPUNIT_ASSERT_EQUAL (BBT_Offset (1, 5, 481), o);

	a = BBT_Time (4,4,959);
	b = BBT_Time (5,3,840);
	o = m.bbt_delta (b, a);

	CPPUNIT_ASSERT_EQUAL (BBT_Offset (0, 10, 841), o);
}
