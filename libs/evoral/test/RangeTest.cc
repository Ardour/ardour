#include "RangeTest.h"
#include "temporal/range.h"
#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION (RangeTest);

using namespace Evoral;

void
RangeTest::coalesceTest ()
{
	RangeList<int> fred;
	fred.add (Range<int> (2, 4));
	fred.add (Range<int> (5, 6));
	fred.add (Range<int> (6, 8));

	RangeList<int>::List jim = fred.get ();

	RangeList<int>::List::iterator i = jim.begin ();
	CPPUNIT_ASSERT_EQUAL (2, i->from);
	CPPUNIT_ASSERT_EQUAL (4, i->to);

	++i;
	CPPUNIT_ASSERT_EQUAL (5, i->from);
	CPPUNIT_ASSERT_EQUAL (8, i->to);
}

/* Basic subtraction of a few smaller ranges from a larger one */
void
RangeTest::subtractTest1 ()
{

/*         01234567890
 * fred:   |---------|
 * jim:      |-|  ||
 * sheila: ||   ||  ||
 */

	Range<int> fred (0, 10);

	RangeList<int> jim;
	jim.add (Range<int> (2, 4));
	jim.add (Range<int> (7, 8));

	RangeList<int> sheila = subtract (fred, jim);

	RangeList<int>::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (3), s.size ());

	RangeList<int>::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (0, i->from);
	CPPUNIT_ASSERT_EQUAL (1, i->to);

	++i;
	CPPUNIT_ASSERT_EQUAL (5, i->from);
	CPPUNIT_ASSERT_EQUAL (6, i->to);

	++i;
	CPPUNIT_ASSERT_EQUAL (9, i->from);
	CPPUNIT_ASSERT_EQUAL (10, i->to);
}

/* Test subtraction of a range B from a range A, where A and B do not overlap */
void
RangeTest::subtractTest2 ()
{
	Range<int> fred (0, 10);

	RangeList<int> jim;
	jim.add (Range<int> (12, 19));

	RangeList<int> sheila = subtract (fred, jim);

	RangeList<int>::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (1), s.size ());

	RangeList<int>::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (0, i->from);
	CPPUNIT_ASSERT_EQUAL (10, i->to);
}

/* Test subtraction of B from A, where B entirely overlaps A */
void
RangeTest::subtractTest3 ()
{
	Range<int> fred (0, 10);

	RangeList<int> jim;
	jim.add (Range<int> (0, 12));

	RangeList<int> sheila = subtract (fred, jim);

	RangeList<int>::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (0), s.size ());
}

/* A bit like subtractTest1, except some of the ranges
   we are subtracting overlap.
*/
void
RangeTest::subtractTest4 ()
{
/*         01234567890
 * fred:   |---------|
 * jim:      |-|  ||
 *                 ||
 * sheila: ||   ||   |
 */

	Range<int> fred (0, 10);

	RangeList<int> jim;
	jim.add (Range<int> (2, 4));
	jim.add (Range<int> (7, 8));
	jim.add (Range<int> (8, 9));

	RangeList<int> sheila = subtract (fred, jim);

	RangeList<int>::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (3), s.size ());

	RangeList<int>::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (0, i->from);
	CPPUNIT_ASSERT_EQUAL (1, i->to);

	++i;
	CPPUNIT_ASSERT_EQUAL (5, i->from);
	CPPUNIT_ASSERT_EQUAL (6, i->to);

	++i;
	CPPUNIT_ASSERT_EQUAL (10, i->from);
	CPPUNIT_ASSERT_EQUAL (10, i->to);
}

/* A bit like subtractTest1, except some of the ranges
   we are subtracting overlap the start / end of the
   initial range.
*/
void
RangeTest::subtractTest5 ()
{
/*         01234567890123
 * fred:    |----------|
 * jim:    |---| || |------...
 * sheila:i     |  |
 */

	Range<int> fred (1, 12);

	RangeList<int> jim;
	jim.add (Range<int> (0, 4));
	jim.add (Range<int> (6, 7));
	jim.add (Range<int> (9, 42));

	RangeList<int> sheila = subtract (fred, jim);

	RangeList<int>::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (2), s.size ());

	RangeList<int>::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (5, i->from);
	CPPUNIT_ASSERT_EQUAL (5, i->to);

	++i;
	CPPUNIT_ASSERT_EQUAL (8, i->from);
	CPPUNIT_ASSERT_EQUAL (8, i->to);
}

/* Test coverage() with all possible types of overlap.
 */

void
RangeTest::coverageTest ()
{

	// b starts before a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 1), Evoral::OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 2), Evoral::OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 3), Evoral::OverlapStart);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 5), Evoral::OverlapStart);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 7), Evoral::OverlapExternal);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 9), Evoral::OverlapExternal);

	// b starts at a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 3), Evoral::OverlapStart);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 5), Evoral::OverlapStart);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 7), Evoral::OverlapExternal);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 9), Evoral::OverlapExternal);

	// b starts inside a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 4), Evoral::OverlapInternal);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 6), Evoral::OverlapInternal);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 7), Evoral::OverlapEnd);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 8), Evoral::OverlapEnd);

	// b starts at end of a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 7, 7), Evoral::OverlapEnd);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 7, 9), Evoral::OverlapEnd);

	// b starts after end of a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 8, 8), Evoral::OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 8, 9), Evoral::OverlapNone);

	// zero-length range a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 3, 2, 4), Evoral::OverlapExternal);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 3, 1, 2), Evoral::OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 3, 3, 3), Evoral::OverlapExternal);
	CPPUNIT_ASSERT_EQUAL (coverage(3, 3, 8, 9), Evoral::OverlapNone);

	// negative length range a
	// XXX these are debatable - should we just consider start & end to be
	// swapped if end < start?
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 1, 2), Evoral::OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 2, 3), Evoral::OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 2, 4), Evoral::OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 3, 3), Evoral::OverlapNone);
	CPPUNIT_ASSERT_EQUAL (coverage(4, 3, 8, 9), Evoral::OverlapNone);

	// negative length range b
	// b starts before a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 1, 0), Evoral::OverlapNone);
	// b starts at a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 3, 2), Evoral::OverlapNone);
	// b starts inside a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 4, 3), Evoral::OverlapNone);
	// b starts at end of a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 7, 5), Evoral::OverlapNone);
	// b starts after end of a
	CPPUNIT_ASSERT_EQUAL (coverage(3, 7, 8, 7), Evoral::OverlapNone);

}
