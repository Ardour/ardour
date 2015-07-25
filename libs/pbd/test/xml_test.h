#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class XMLTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (XMLTest);
	CPPUNIT_TEST (testXMLFilenameEncoding);
	CPPUNIT_TEST_SUITE_END ();

public:
	void testXMLFilenameEncoding ();
};
