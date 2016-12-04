#include "NoteTest.hpp"
#include "evoral/Note.hpp"
#include "evoral/Beats.hpp"
#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION (NoteTest);

using namespace Evoral;

typedef Beats Time;

void
NoteTest::copyTest ()
{
	Note<Time> a(0, Beats(1.0), Beats(2.0), 60, 0x40);
	Note<Time> b(a);
	CPPUNIT_ASSERT (a == b);

	// Broken due to event double free!
	// Note<Time> c(1, Beats(3.0), Beats(4.0), 61, 0x41);
	// c = a;
	// CPPUNIT_ASSERT (a == c);
}

void
NoteTest::idTest ()
{
	Note<Time> a(0, Beats(1.0), Beats(2.0), 60, 0x40);
	CPPUNIT_ASSERT_EQUAL (-1, a.id());

	a.set_id(1234);
	CPPUNIT_ASSERT_EQUAL (1234, a.id());
}
