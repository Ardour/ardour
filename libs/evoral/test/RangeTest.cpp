#include "RangeTest.hpp"
#include "evoral/Range.hpp"

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
