#include <cppunit/TestFixture.h>
#include "pbd/properties.h"

class ScalarPropertiesTest : public CppUnit::TestFixture
{
public:
	CPPUNIT_TEST_SUITE (ScalarPropertiesTest);
	CPPUNIT_TEST (testBasic);
	CPPUNIT_TEST_SUITE_END ();

	void testBasic ();

private:
	PBD::Property<int> _property;
};
