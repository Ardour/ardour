#include "scalar_properties.h"

using namespace PBD;

namespace Properties {
	PBD::PropertyDescriptor<int> fred;
};

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

	_fred = 5;
	CPPUNIT_ASSERT (_fred == 5);
	CPPUNIT_ASSERT (_fred.changed() == true);

	PropertyList undo;
	PropertyList redo;
	_fred.diff (undo, redo);
}
