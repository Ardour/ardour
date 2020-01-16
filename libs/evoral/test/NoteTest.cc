#include "NoteTest.h"
#include "temporal/beats.h"
#include "evoral/Note.h"
#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION (NoteTest);

using namespace Evoral;

typedef Temporal::Beats Time;

void
NoteTest::copyTest ()
{
	Note<Time> a(0, Time(1.0), Time(2.0), 60, 0x40);
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
	Note<Time> a(0, Time(1.0), Time(2.0), 60, 0x40);
	CPPUNIT_ASSERT_EQUAL (-1, a.id());

	a.set_id(1234);
	CPPUNIT_ASSERT_EQUAL (1234, a.id());
}
