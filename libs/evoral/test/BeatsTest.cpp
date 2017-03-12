#include "BeatsTest.hpp"
#include "evoral/Beats.hpp"
#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION (BeatsTest);

using namespace Evoral;

void
BeatsTest::operator_eq ()
{
	for (int i = 1; i < 1000; i++)
		CPPUNIT_ASSERT (Beats::ticks(i-1).operator!=(Beats::ticks(i)));
}
