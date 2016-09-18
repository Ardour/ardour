#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class FilesystemTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (FilesystemTest);
	CPPUNIT_TEST (testPathIsWithin);
	CPPUNIT_TEST (testCopyFileASCIIFilename);
	CPPUNIT_TEST (testCopyFileUTF8Filename);
	CPPUNIT_TEST (testOpenFileUTF8Filename);
	CPPUNIT_TEST (testFindFilesMatchingPattern);
	CPPUNIT_TEST (testClearDirectory);
	CPPUNIT_TEST (testRemoveDirectory);
	CPPUNIT_TEST (testCanonicalPathASCII);
	CPPUNIT_TEST (testCanonicalPathUTF8);
	CPPUNIT_TEST (testTouchFile);
	CPPUNIT_TEST (testStatFile);
	CPPUNIT_TEST_SUITE_END ();

public:
	void testPathIsWithin ();
	void testCopyFileASCIIFilename ();
	void testCopyFileUTF8Filename ();
	void testOpenFileUTF8Filename ();
	void testFindFilesMatchingPattern ();
	void testClearDirectory ();
	void testRemoveDirectory ();
	void testCanonicalPathASCII ();
	void testCanonicalPathUTF8 ();
	void testTouchFile ();
	void testStatFile ();
};

