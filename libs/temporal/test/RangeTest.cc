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
	timepos_t t5 (5);
	timepos_t t6 (6);
	timepos_t t7 (7);
	timepos_t t9 (9);

	RangeList fred;
	fred.add (Range (t2, t5));
	fred.add (Range (t5, t7));
	fred.add (Range (t6, t9));

	RangeList::List jim = fred.get ();

	RangeList::List::iterator i = jim.begin ();
	CPPUNIT_ASSERT_EQUAL (t2, i->start());
	CPPUNIT_ASSERT_EQUAL (t5, i->end());
	++i;
	CPPUNIT_ASSERT_EQUAL (t5, i->start());
	CPPUNIT_ASSERT_EQUAL (t9, i->end());
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
	jim.add (Range (t2, t5));
	jim.add (Range (t7, t9));

	RangeList sheila = fred.subtract (jim);

	RangeList::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (3), s.size ());

	RangeList::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (t0, i->start());
	CPPUNIT_ASSERT_EQUAL (t2, i->end()); // XXX -> 2

	++i;
	CPPUNIT_ASSERT_EQUAL (t5, i->start()); // XXX -> 4
	CPPUNIT_ASSERT_EQUAL (t7, i->end());   // XXX -> 7

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
	timepos_t t11 (11);

/*         01234567890
 * fred:   |---------|
 * jim:      |-|  ||
 *                 ||
 * sheila: ||   ||   |
 */

	Range fred (t0, t11);;

	RangeList jim;
	jim.add (Range (t2, t5));
	jim.add (Range (t7, t9));
	jim.add (Range (t8, t10));

	RangeList sheila = fred.subtract (jim);

	RangeList::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (3), s.size ());

	RangeList::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (t0, i->start());
	CPPUNIT_ASSERT_EQUAL (t2, i->end());

	++i;
	CPPUNIT_ASSERT_EQUAL (t5, i->start());
	CPPUNIT_ASSERT_EQUAL (t7, i->end());

	++i;
	CPPUNIT_ASSERT_EQUAL (t10, i->start());
	CPPUNIT_ASSERT_EQUAL (t11, i->end());
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
	timepos_t t13 (13);
	timepos_t t43 (43);

/*         01234567890123
 * fred:    |----------|
 * jim:    |---| || |------...
 * sheila:i     |  |
 */

	Range fred (t1, t13);

	RangeList jim;
	jim.add (Range (t0, t5));
	jim.add (Range (t6, t8));
	jim.add (Range (t9, t43));

	RangeList sheila = fred.subtract (jim);

	RangeList::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (2), s.size ());

	RangeList::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (t5, i->start());
	CPPUNIT_ASSERT_EQUAL (t6, i->end());

	++i;
	CPPUNIT_ASSERT_EQUAL (t8, i->start());
	CPPUNIT_ASSERT_EQUAL (t9, i->end());
}

/* Test coverage() with all possible types of overlap.
 */

void
RangeTest::coverageTest ()
{
#define coverage(A0, A1, B0, B1) Range(timepos_t(A0), timepos_t(A1)).coverage(timepos_t(B0), timepos_t(B1))

	// b starts before a
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 1, 2), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 1, 3), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 1, 4), OverlapStart); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 1, 6), OverlapStart);
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 1, 8), OverlapExternal);
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 1, 10), OverlapExternal);

	// b starts at a
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 3, 4), OverlapStart); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 3, 6), OverlapStart);
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 3, 8), OverlapExternal);
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 3, 10), OverlapExternal);

	// b starts inside a
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 4, 5), OverlapInternal); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 4, 7), OverlapInternal);
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 4, 8), OverlapEnd);
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 4, 9), OverlapEnd);

	// b starts at end of a
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 7, 8), OverlapEnd); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 7, 10), OverlapEnd); // XXX fails

	// b starts after end of a
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 8, 9), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 8, 10), OverlapNone);

	// zero-length range a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 4, 2, 5), OverlapExternal); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage(3, 4, 1, 3), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 4, 3, 4), OverlapExternal); // XXX fails
	CPPUNIT_ASSERT_EQUAL (coverage(3, 4, 8, 10), OverlapNone);

	// negative length range a
	// XXX these are debatable - should we just consider start & end to be
	// swapped if end < start?
	CPPUNIT_ASSERT_EQUAL (coverage(4, 4, 1, 3), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 4, 2, 4), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 4, 2, 5), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 4, 3, 4), OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 4, 8, 10), OverlapNone);

	// negative length range b
	// b starts before a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 8, 1, 1), OverlapNone);
	// b starts at a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 8, 3, 3), OverlapNone);
	// b starts inside a
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 4, 4), OverlapNone);
	// b starts at end of a
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 7, 6), OverlapNone);
	// b starts after end of a
	CPPUNIT_ASSERT_EQUAL (coverage (3, 8, 8, 8), OverlapNone);

}
