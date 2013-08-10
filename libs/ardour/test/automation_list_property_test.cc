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

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/properties.h"
#include "pbd/stateful_diff_command.h"
#include "ardour/automation_list.h"
#include "automation_list_property_test.h"
#include "test_util.h"
#include "test_common.h"

CPPUNIT_TEST_SUITE_REGISTRATION (AutomationListPropertyTest);

using namespace std;
using namespace PBD;
using namespace ARDOUR;

void
AutomationListPropertyTest::basicTest ()
{
	list<string> ignore_properties;
	ignore_properties.push_back ("id");

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

	std::string test_data_file1 = Glib::build_filename (test_search_path().front(), "automation_list_property_test1.ref");
	CPPUNIT_ASSERT (Glib::file_test (test_data_file1, Glib::FILE_TEST_EXISTS));

	XMLNode* foo = new XMLNode ("test");
	property.get_changes_as_xml (foo);
	check_xml (foo, test_data_file1, ignore_properties);

	std::string test_data_file2 = Glib::build_filename (test_search_path().front(), "automation_list_property_test2.ref");
	CPPUNIT_ASSERT (Glib::file_test (test_data_file2, Glib::FILE_TEST_EXISTS));

	/* Do some more */
	property.clear_changes ();
	CPPUNIT_ASSERT_EQUAL (false, property.changed());
	property->add (5, 6);
	property->add (7, 8);
	CPPUNIT_ASSERT_EQUAL (true, property.changed());
	foo = new XMLNode ("test");
	property.get_changes_as_xml (foo);
	check_xml (foo, test_data_file2, ignore_properties);
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
	list<string> ignore_properties;
	ignore_properties.push_back ("id");

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

	std::string test_data_file3 = Glib::build_filename (test_search_path().front(), "automation_list_property_test3.ref");
	CPPUNIT_ASSERT (Glib::file_test (test_data_file3, Glib::FILE_TEST_EXISTS));

	/* Undo */
	sdc.undo ();
	check_xml (&sheila->get_state(), test_data_file3, ignore_properties);

	std::string test_data_file4 = Glib::build_filename (test_search_path().front(), "automation_list_property_test4.ref");
	CPPUNIT_ASSERT (Glib::file_test (test_data_file4, Glib::FILE_TEST_EXISTS));

	/* Redo */
	sdc.redo ();
	check_xml (&sheila->get_state(), test_data_file4, ignore_properties);
}
