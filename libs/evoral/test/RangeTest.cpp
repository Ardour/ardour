#include "RangeTest.hpp"
#include "evoral/Range.hpp"
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
	CPPUNIT_ASSERT_EQUAL (4, i->from);
	CPPUNIT_ASSERT_EQUAL (6, i->to);

	++i;
	CPPUNIT_ASSERT_EQUAL (8, i->from);
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
	CPPUNIT_ASSERT_EQUAL (4, i->from);
	CPPUNIT_ASSERT_EQUAL (6, i->to);

	++i;
	CPPUNIT_ASSERT_EQUAL (9, i->from);
	CPPUNIT_ASSERT_EQUAL (10, i->to);
}

/* A bit like subtractTest1, except some of the ranges
   we are subtracting overlap the start / end of the
   initial range.
*/
void
RangeTest::subtractTest5 ()
{
	Range<int> fred (1, 12);

	RangeList<int> jim;
	jim.add (Range<int> (0, 4));
	jim.add (Range<int> (6, 7));
	jim.add (Range<int> (9, 42));

	RangeList<int> sheila = subtract (fred, jim);

	RangeList<int>::List s = sheila.get ();
	CPPUNIT_ASSERT_EQUAL (size_t (2), s.size ());

	RangeList<int>::List::iterator i = s.begin ();
	CPPUNIT_ASSERT_EQUAL (4, i->from);
	CPPUNIT_ASSERT_EQUAL (5, i->to);

	++i;
	CPPUNIT_ASSERT_EQUAL (7, i->from);
	CPPUNIT_ASSERT_EQUAL (8, i->to);
}
