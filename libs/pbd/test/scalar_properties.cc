#include "scalar_properties.h"

using namespace PBD;

ScalarPropertiesTest::testBasic ()
{
	CPPUNIT_ASSERT (_property.changed() == false);
	
	_property = 4;
	CPPUNIT_ASSERT (_property == 4);
	CPPUNIT_ASSERT (_property.changed() == true);

	_property = 5;
	CPPUNIT_ASSERT (_property == 5);
	CPPUNIT_ASSERT (_property.changed() == true);

	PropertyList undo;
	PropertyList redo;
	_property.diff (undo, redo);
}
