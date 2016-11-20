#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class XMLTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (XMLTest);
	CPPUNIT_TEST (testXMLFilenameEncoding);
	CPPUNIT_TEST (testPerfSmallXMLDocument);
	CPPUNIT_TEST (testPerfMediumXMLDocument);
	CPPUNIT_TEST (testPerfLargeXMLDocument);
	CPPUNIT_TEST_SUITE_END ();

public:
	void testXMLFilenameEncoding ();
	void testPerfSmallXMLDocument ();
	void testPerfMediumXMLDocument ();
	void testPerfLargeXMLDocument ();
};
