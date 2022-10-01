#include "RangeTest.h"
#include "temporal/range.h"
#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION (RangeTest);

using namespace Evoral;
using namespace Temporal;

void
RangeTest::coalesceTest ()
{
	timepos_t t2 (2);
	timepos_t t4 (4);
	timepos_t t5 (5);
	timepos_t t6 (6);
	timepos_t t8 (8);

	RangeList fred;
	fred.add (Range (t2, t4));
	fred.add (Range (t5, t6));
	fred.add (Range (t6, t8));

	RangeList::List jim = fred.get ();

	RangeList::List::iterator i = jim.begin ();
	CPPUNIT_ASSERT_EQUAL (t2, i->start());
	CPPUNIT_ASSERT_EQUAL (t4, i->end());
	++i;
	CPPUNIT_ASSERT_EQUAL (t5, i->start());
	CPPUNIT_ASSERT_EQUAL (t8, i->end());
}

/* Basic subtraction of a few smaller ranges from a larger one */
void
RangeTest::subtractTest1 ()
{

	timepos_t t0 (0);
	timepos_t t1 (1);
	timepos_t t2 (2);
	timepos_t t4 (4);
	timepos_t t5 (5);
	timepos_t t6 (6);
	timepos_t t7 (7);
	timepos_t t8 (8);
	timepos_t t9 (9);
	timepos_t t10 (10);

/*         01234567890
 * fred:   |---------|
 * jim:      |-|  ||
 * sheila: ||   ||  ||
 */

	Range fred (t0, t10);

	RangeList jim;
	jim.add (Range (t2, t4));
	jim.add (Range (t7, t8));

	RangeList sheila = fred.subtract (jim);

	RangeList::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (3), s.size ());

	RangeList::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (t0, i->start());
	CPPUNIT_ASSERT_EQUAL (t1, i->end()); // XXX -> 2

	++i;
	CPPUNIT_ASSERT_EQUAL (t5, i->start()); // XXX -> 4
	CPPUNIT_ASSERT_EQUAL (t6, i->end());   // XXX -> 7

	++i;
	CPPUNIT_ASSERT_EQUAL (t9, i->start()); // XXX -> 8
	CPPUNIT_ASSERT_EQUAL (t10, i->end());
}

/* Test subtraction of a range B from a range A, where A and B do not overlap */
void
RangeTest::subtractTest2 ()
{
	timepos_t t0 (0);
	timepos_t t10 (10);
	timepos_t t12 (12);
	timepos_t t19 (19);

	Range fred (t0, t10);

	RangeList jim;
	jim.add (Range (t12, t19));

	RangeList sheila = fred.subtract (jim);

	RangeList::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (1), s.size ());

	RangeList::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (t0, i->start());
	CPPUNIT_ASSERT_EQUAL (t10, i->end());
}

/* Test subtraction of B from A, where B entirely overlaps A */
void
RangeTest::subtractTest3 ()
{
	timepos_t t0 (0);
	timepos_t t10 (10);
	timepos_t t12 (12);
	Range fred (t0, t10);

	RangeList jim;
	jim.add (Range (t0, t12));

	RangeList sheila = fred.subtract (jim);

	RangeList::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (0), s.size ());
}

/* A bit like subtractTest1, except some of the ranges
   we are subtracting overlap.
*/
void
RangeTest::subtractTest4 ()
{
	timepos_t t0 (0);
	timepos_t t1 (1);
	timepos_t t2 (2);
	timepos_t t4 (4);
	timepos_t t5 (5);
	timepos_t t6 (6);
	timepos_t t7 (7);
	timepos_t t8 (8);
	timepos_t t9 (9);
	timepos_t t10 (10);

/*         01234567890
 * fred:   |---------|
 * jim:      |-|  ||
 *                 ||
 * sheila: ||   ||   |
 */

	Range fred (t0, t10);

	RangeList jim;
	jim.add (Range (t2, t4));
	jim.add (Range (t7, t8));
	jim.add (Range (t8, t9));

	RangeList sheila = fred.subtract (jim);

	RangeList::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (3), s.size ());

	RangeList::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (t0, i->start());
	CPPUNIT_ASSERT_EQUAL (t1, i->end());

	++i;
	CPPUNIT_ASSERT_EQUAL (t5, i->start());
	CPPUNIT_ASSERT_EQUAL (t6, i->end());

	++i;
	CPPUNIT_ASSERT_EQUAL (t10, i->start());
	CPPUNIT_ASSERT_EQUAL (t10, i->end());
}

/* A bit like subtractTest1, except some of the ranges
   we are subtracting overlap the start / end of the
   initial range.
*/
void
RangeTest::subtractTest5 ()
{
	timepos_t t0 (0);
	timepos_t t1 (1);
	timepos_t t4 (4);
	timepos_t t5 (5);
	timepos_t t6 (6);
	timepos_t t7 (7);
	timepos_t t8 (8);
	timepos_t t9 (9);
	timepos_t t12 (12);
	timepos_t t42 (42);

/*         01234567890123
 * fred:    |----------|
 * jim:    |---| || |------...
 * sheila:i     |  |
 */

	Range fred (t1, t12);

	RangeList jim;
	jim.add (Range (t0, t4));
	jim.add (Range (t6, t7));
	jim.add (Range (t9, t42));

	RangeList sheila = fred.subtract (jim);

	RangeList::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (2), s.size ());

	RangeList::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (t5, i->start());
	CPPUNIT_ASSERT_EQUAL (t5, i->end());

	++i;
	CPPUNIT_ASSERT_EQUAL (t8, i->start());
	CPPUNIT_ASSERT_EQUAL (t8, i->end());
}

/* Test coverage() with all possible types of overlap.
 */

void
RangeTest::coverageTest ()
{
#define coverage(A0, A1, B0, B1) Range(timepos_t(A0), timepos_t(A1)).coverage(timepos_t(B0), timepos_t(B1))

	// b starts before a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 1), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 2), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 3), OverlapStart); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 5), OverlapStart);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 7), OverlapExternal);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 9), OverlapExternal);

	// b starts at a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 3), OverlapStart); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 5), OverlapStart);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 7), OverlapExternal);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 9), OverlapExternal);

	// b starts inside a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 4), OverlapInternal); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 6), OverlapInternal);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 7), OverlapEnd);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 8), OverlapEnd);

	// b starts at end of a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 7, 7), OverlapEnd); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 7, 9), OverlapEnd); // XXX fails

	// b starts after end of a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 8, 8), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 8, 9), OverlapNone);

	// zero-length range a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 3, 2, 4), OverlapExternal); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage(3, 3, 1, 2), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 3, 3, 3), OverlapExternal); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage(3, 3, 8, 9), OverlapNone);

	// negative length range a
	// XXX these are debatable - should we just consider start & end to be
	// swapped if end < start?
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 1, 2), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 2, 3), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 2, 4), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 3, 3), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 8, 9), OverlapNone);

	// negative length range b
	// b starts before a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 0), OverlapNone);
	// b starts at a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 2), OverlapNone);
	// b starts inside a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 3), OverlapNone);
	// b starts at end of a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 7, 5), OverlapNone);
	// b starts after end of a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 8, 7), OverlapNone);

}
