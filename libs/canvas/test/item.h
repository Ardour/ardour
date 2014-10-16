#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ItemTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ItemTest);
	CPPUNIT_TEST (item_to_canvas);
	CPPUNIT_TEST_SUITE_END ();

public:
	void item_to_canvas ();
};
