#include "scalar_properties.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ScalarPropertiesTest);

using namespace std;
using namespace PBD;

namespace Properties {
	PBD::PropertyDescriptor<int> fred;
};

void
ScalarPropertiesTest::make_property_quarks ()
{
	Properties::fred.property_id = g_quark_from_static_string ("fred");
}

ScalarPropertiesTest::ScalarPropertiesTest ()
	: _fred (Properties::fred, 0)
{
}

void
ScalarPropertiesTest::testBasic ()
{
	CPPUNIT_ASSERT (_fred.changed() == false);

	_fred = 4;
	CPPUNIT_ASSERT (_fred == 4);
	CPPUNIT_ASSERT (_fred.changed() == true);

	_fred.clear_changes ();
	CPPUNIT_ASSERT (_fred.changed() == false);

	_fred = 5;
	CPPUNIT_ASSERT (_fred == 5);
	CPPUNIT_ASSERT (_fred.changed() == true);

	PropertyList changes;
	_fred.get_changes_as_properties (changes, 0);

	CPPUNIT_ASSERT (changes.size() == 1);

	PropertyTemplate<int>* t = dynamic_cast<Property<int>*> (changes.begin()->second);
	CPPUNIT_ASSERT (t);
	CPPUNIT_ASSERT (t->val() == 5);
}
