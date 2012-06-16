/*
    Copyright (C) 2012 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "pbd/properties.h"
#include "pbd/stateful_diff_command.h"
#include "ardour/automation_list.h"
#include "automation_list_property_test.h"
#include "test_util.h"

CPPUNIT_TEST_SUITE_REGISTRATION (AutomationListPropertyTest);

using namespace std;
using namespace PBD;
using namespace ARDOUR;

void
AutomationListPropertyTest::basicTest ()
{
	PropertyDescriptor<boost::shared_ptr<AutomationList> > descriptor;
	descriptor.property_id = g_quark_from_static_string ("FadeIn");
	AutomationListProperty property (
		descriptor,
		boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeInAutomation)))
		);

	property.clear_changes ();

	/* No change since we just cleared them */
	CPPUNIT_ASSERT_EQUAL (false, property.changed());
	
	property->add (1, 2);
	property->add (3, 4);

	/* Now it has changed */
	CPPUNIT_ASSERT_EQUAL (true, property.changed());

	XMLNode* foo = new XMLNode ("test");
	property.get_changes_as_xml (foo);
	check_xml (foo, "../libs/ardour/test/data/automation_list_property_test1.ref");

	/* Do some more */
	property.clear_changes ();
	CPPUNIT_ASSERT_EQUAL (false, property.changed());
	property->add (5, 6);
	property->add (7, 8);
	CPPUNIT_ASSERT_EQUAL (true, property.changed());
	foo = new XMLNode ("test");
	property.get_changes_as_xml (foo);
	check_xml (foo, "../libs/ardour/test/data/automation_list_property_test2.ref");
}

/** Here's a StatefulDestructible class that has a AutomationListProperty */
class Fred : public StatefulDestructible
{
public:
	Fred ()
		: _jim (_descriptor, boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeInAutomation))))

	{
		add_property (_jim);
	}

	XMLNode & get_state () {
		XMLNode* n = new XMLNode ("State");
		add_properties (*n);
		return *n;
	}

	int set_state (XMLNode const & node, int) {
		set_values (node);
		return 0;
	}

	static void make_property_quarks () {
		_descriptor.property_id = g_quark_from_static_string ("FadeIn");
	}

	AutomationListProperty _jim;
	static PropertyDescriptor<boost::shared_ptr<AutomationList> > _descriptor;
};

PropertyDescriptor<boost::shared_ptr<AutomationList> > Fred::_descriptor;

void
AutomationListPropertyTest::undoTest ()
{
	Fred::make_property_quarks ();

	boost::shared_ptr<Fred> sheila (new Fred);

	/* Add some data */
	sheila->_jim->add (1, 2);
	sheila->_jim->add (3, 4);

	/* Do a `command' */
	sheila->clear_changes ();
	sheila->_jim->add (5, 6);
	sheila->_jim->add (7, 8);
	StatefulDiffCommand sdc (sheila);

	/* Undo */
	sdc.undo ();
	check_xml (&sheila->get_state(), "../libs/ardour/test/data/automation_list_property_test3.ref");

	/* Redo */
	sdc.redo ();
	check_xml (&sheila->get_state(), "../libs/ardour/test/data/automation_list_property_test4.ref");
}
