#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "pbd/properties.h"

class ScalarPropertiesTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ScalarPropertiesTest);
	CPPUNIT_TEST (testBasic);
	CPPUNIT_TEST_SUITE_END ();

public:
	ScalarPropertiesTest ();
	void testBasic ();

	static void make_property_quarks ();

private:
	PBD::Property<int> _fred;
};
