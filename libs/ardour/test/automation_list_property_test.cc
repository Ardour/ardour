/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

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
write_automation_list_xml (XMLNode* node, std::string filename)
{
	// use the same output dir for all of them
	static std::string test_output_dir = new_test_output_dir ("automation_list_property");
	std::string output_file = Glib::build_filename (test_output_dir, filename);

	CPPUNIT_ASSERT (write_ref (node, output_file));
}

static int
static_sample_rate () { return 48000; }

void
AutomationListPropertyTest::setUp ()
{
	Temporal::set_sample_rate_callback (static_sample_rate);
}

void
AutomationListPropertyTest::tearDown ()
{
	Temporal::set_sample_rate_callback (0);
}

void
AutomationListPropertyTest::basicTest ()
{
	list<string> ignore_properties;
	ignore_properties.push_back ("id");

	PropertyDescriptor<boost::shared_ptr<AutomationList> > descriptor;
	descriptor.property_id = g_quark_from_static_string ("FadeIn");
	AutomationListProperty property (
		descriptor,
		boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeInAutomation), Temporal::AudioTime))
		);

	property.clear_changes ();

	/* No change since we just cleared them */
	CPPUNIT_ASSERT_EQUAL (false, property.changed());

	property->add (timepos_t(1), 0.5, false, false);
	property->add (timepos_t(3), 2.0, false, false);

	/* Now it has changed */
	CPPUNIT_ASSERT_EQUAL (true, property.changed());

	std::string test_data_filename = "automation_list_property_test1.ref";
	std::string test_data_file1 = Glib::build_filename (test_search_path().front(), test_data_filename);
	CPPUNIT_ASSERT (Glib::file_test (test_data_file1, Glib::FILE_TEST_EXISTS));

	XMLNode* foo = new XMLNode ("test");
	property.get_changes_as_xml (foo);
	write_automation_list_xml (foo, test_data_filename);
	check_xml (foo, test_data_file1, ignore_properties);

	test_data_filename = "automation_list_property_test2.ref";
	std::string test_data_file2 = Glib::build_filename (test_search_path().front(), test_data_filename);
	CPPUNIT_ASSERT (Glib::file_test (test_data_file2, Glib::FILE_TEST_EXISTS));

	/* Do some more */
	property.clear_changes ();
	CPPUNIT_ASSERT_EQUAL (false, property.changed());
	property->add (timepos_t(5), 1.5, false, false);
	property->add (timepos_t(7), 1.0, false, false);
	CPPUNIT_ASSERT_EQUAL (true, property.changed());
	delete foo;
	foo = new XMLNode ("test");
	property.get_changes_as_xml (foo);
	write_automation_list_xml (foo, test_data_filename);
	check_xml (foo, test_data_file2, ignore_properties);
	delete foo;
}

/** Here's a StatefulDestructible class that has a AutomationListProperty */
class Fred : public StatefulDestructible
{
public:
	Fred ()
		: _jim (_descriptor, boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeInAutomation), Temporal::AudioTime)))

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
	sheila->_jim->add (timepos_t(0), 1, false, false);
	sheila->_jim->add (timepos_t(1), 2, false, false);

	/* Do a `command' */
	sheila->clear_changes ();
	sheila->_jim->add (timepos_t(2), 1, false, false);
	sheila->_jim->add (timepos_t(3), 0, false, false);
	StatefulDiffCommand sdc (sheila);

	std::string test_data_filename = "automation_list_property_test3.ref";
	std::string test_data_file3 = Glib::build_filename (test_search_path().front(), test_data_filename);
	CPPUNIT_ASSERT (Glib::file_test (test_data_file3, Glib::FILE_TEST_EXISTS));

	/* Undo */
	sdc.undo ();
	write_automation_list_xml (&sheila->get_state(), test_data_filename);
	check_xml (&sheila->get_state(), test_data_file3, ignore_properties);

	test_data_filename = "automation_list_property_test4.ref";
	std::string test_data_file4 = Glib::build_filename (test_search_path().front(), test_data_filename);
	CPPUNIT_ASSERT (Glib::file_test (test_data_file4, Glib::FILE_TEST_EXISTS));

	/* Redo */
	sdc.redo ();
	write_automation_list_xml (&sheila->get_state(), test_data_filename);
	check_xml (&sheila->get_state(), test_data_file4, ignore_properties);
}
