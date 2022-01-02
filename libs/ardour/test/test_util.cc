/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2016 Tim Mayberry <mojofunk@gmail.com>
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
#include <sstream>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/file_utils.h"

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"

#include "test_util.h"

#include <cppunit/extensions/HelperMacros.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static void
check_nodes (XMLNode const * p, XMLNode const * q, list<string> const & ignore_properties)
{
	CPPUNIT_ASSERT_EQUAL (q->is_content(), p->is_content());
	if (!p->is_content()) {
		CPPUNIT_ASSERT_EQUAL (q->name(), p->name());
	} else {
		CPPUNIT_ASSERT_EQUAL (q->content(), p->content());
	}

	XMLPropertyList const & pp = p->properties ();
	XMLPropertyList const & qp = q->properties ();
	CPPUNIT_ASSERT_EQUAL (qp.size(), pp.size());

	XMLPropertyList::const_iterator i = pp.begin ();
	XMLPropertyList::const_iterator j = qp.begin ();
	while (i != pp.end ()) {
		CPPUNIT_ASSERT_EQUAL ((*j)->name(), (*i)->name());
		if (find (ignore_properties.begin(), ignore_properties.end(), (*i)->name ()) == ignore_properties.end ()) {
			CPPUNIT_ASSERT_EQUAL_MESSAGE ((*j)->name(), (*i)->value(), (*i)->value());
		}
		++i;
		++j;
	}

	XMLNodeList const & pc = p->children ();
	XMLNodeList const & qc = q->children ();

	CPPUNIT_ASSERT_EQUAL (qc.size(), pc.size());
	XMLNodeList::const_iterator k = pc.begin ();
	XMLNodeList::const_iterator l = qc.begin ();

	while (k != pc.end ()) {
		check_nodes (*k, *l, ignore_properties);
		++k;
		++l;
	}
}

void
check_xml (XMLNode* node, string ref_file, list<string> const & ignore_properties)
{
	XMLTree ref (ref_file);

	XMLNode* p = node;
	XMLNode* q = ref.root ();

	check_nodes (p, q, ignore_properties);
}

bool
write_ref (XMLNode* node, string ref_file)
{
	XMLTree ref;
	ref.set_root (node);
	bool rv = ref.write (ref_file);
	ref.set_root (0);
	return rv;
}

void
create_and_start_dummy_backend ()
{
	AudioEngine* engine = AudioEngine::create ();

	CPPUNIT_ASSERT (AudioEngine::instance ());
	CPPUNIT_ASSERT (engine);
	CPPUNIT_ASSERT (engine->set_backend ("None (Dummy)", "Unit-Test", ""));

	CPPUNIT_ASSERT (engine->start () == 0);
	Temporal::set_sample_rate_callback (AudioEngine::static_sample_rate);
}

void
stop_and_destroy_backend ()
{
	AudioEngine::instance()->remove_session ();
	AudioEngine::instance()->stop ();
	AudioEngine::destroy ();
	Temporal::set_sample_rate_callback (0);
}

/** @param dir Session directory.
 *  @param state Session state file, without .ardour suffix.
 */
Session *
load_session (string dir, string state)
{
	Session* session = new Session (*AudioEngine::instance(), dir, state, 0, "", true);
	AudioEngine::instance ()->set_session (session);
	return session;
}

PBD::Searchpath
test_search_path ()
{
#ifdef PLATFORM_WINDOWS
	if (!getenv("ARDOUR_TEST_PATH")) {
		std::string wsp(windows_package_directory_path());
		return Glib::build_filename (wsp, "ardour_testdata");
	}
#endif
	return Glib::getenv("ARDOUR_TEST_PATH");
}

std::string
new_test_output_dir (std::string prefix)
{
	return PBD::tmp_writable_directory (PACKAGE, prefix);
}

int
get_test_sample_rate ()
{
	return 44100;
}

void
get_utf8_test_strings (std::vector<std::string>& result)
{
	// These are all translations of "Ardour" from google translate
	result.push_back ("Ardour"); // Reference
	result.push_back ("\320\277\321\213\320\273"); // Russian
	result.push_back ("\305\276ar"); // Croatian
	result.push_back ("\340\270\204\340\270\247\340\270\262\340\270\241\340\270\201\340\270\243\340\270\260\340\270\225\340\270\267\340\270\255\340\270\243\340\270\267\340\270\255\340\270\243\340\271\211\340\270\231"); // Thai
	result.push_back ("\325\245\325\274\325\241\325\266\325\244"); // Armenian
	result.push_back ("\340\246\254\340\247\215\340\246\257\340\246\227\340\247\215\340\246\260\340\246\244\340\246\276"); // Bengali
	result.push_back ("\346\203\205\347\206\261"); // Japanese
	result.push_back ("\347\203\255\346\203\205"); // Chinese (Simplified)
}
