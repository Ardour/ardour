#include "convert_test.h"
#include "pbd/convert.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ConvertTest);

using namespace std;

void
ConvertTest::testUrlDecode ()
{
	string url = "http://foo.bar.baz/A%20B%20C%20.html";
	PBD::url_decode (url);
	CPPUNIT_ASSERT_EQUAL (url, string ("http://foo.bar.baz/A B C .html"));
}
