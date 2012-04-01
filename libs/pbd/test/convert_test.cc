#include "convert_test.h"
#include "pbd/convert.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ConvertTest);

using namespace std;

void
ConvertTest::testUrlDecode ()
{
	string const url = "http://foo.bar.baz/A%20B%20C%20+42.html";
	CPPUNIT_ASSERT_EQUAL (PBD::url_decode (url), string ("http://foo.bar.baz/A B C  42.html"));
}
