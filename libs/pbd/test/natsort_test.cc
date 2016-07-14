#include "natsort_test.h"
#include "pbd/natsort.h"

CPPUNIT_TEST_SUITE_REGISTRATION (NatSortTest);

using namespace std;


void
NatSortTest::testBasic ()
{
	CPPUNIT_ASSERT (!PBD::naturally_less ("a32", "a4"));
	CPPUNIT_ASSERT (!PBD::naturally_less ("a32", "a04"));
	CPPUNIT_ASSERT ( PBD::naturally_less ("a32", "a40"));
	CPPUNIT_ASSERT ( PBD::naturally_less ("a32a", "a32b"));
	CPPUNIT_ASSERT (!PBD::naturally_less ("a32b", "a32a"));
	CPPUNIT_ASSERT (!PBD::naturally_less ("abcd", "abc"));
	CPPUNIT_ASSERT ( PBD::naturally_less ("abc", "abcd"));
	CPPUNIT_ASSERT (!PBD::naturally_less ("abc", "abc"));
}
