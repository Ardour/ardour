#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class FilesystemTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (FilesystemTest);
	CPPUNIT_TEST (testPathIsWithin);
	CPPUNIT_TEST (testCopyFileASCIIFilename);
	CPPUNIT_TEST (testCopyFileUTF8Filename);
	CPPUNIT_TEST (testFindFilesMatchingPattern);
	CPPUNIT_TEST_SUITE_END ();

public:
	void testPathIsWithin ();
	void testCopyFileASCIIFilename ();
	void testCopyFileUTF8Filename ();
	void testFindFilesMatchingPattern ();
};

