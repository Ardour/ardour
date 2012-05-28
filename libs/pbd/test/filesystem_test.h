#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class FilesystemTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (FilesystemTest);
	CPPUNIT_TEST (testPathIsWithin);
	CPPUNIT_TEST_SUITE_END ();

public:
	void testPathIsWithin ();

};

