#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class NoteTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (NoteTest);
	CPPUNIT_TEST (copyTest);
	CPPUNIT_TEST (idTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void copyTest ();
	void idTest ();
};


