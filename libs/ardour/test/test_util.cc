/*
    Copyright (C) 2011 Paul Davis
    Copyright (C) 2011 Tim Mayberry

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include <fstream>
#include <sstream>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/textreceiver.h"
#include "pbd/file_utils.h"

#include "ardour/session.h"
#include "ardour/audioengine.h"

#include "test_util.h"

#include <cppunit/extensions/HelperMacros.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static void
check_nodes (XMLNode const * p, XMLNode const * q, list<string> const & ignore_properties)
{
	CPPUNIT_ASSERT_EQUAL (p->is_content(), q->is_content());
	if (!p->is_content()) {
		CPPUNIT_ASSERT_EQUAL (p->name(), q->name());
	} else {
		CPPUNIT_ASSERT_EQUAL (p->content(), q->content());
	}

	XMLPropertyList const & pp = p->properties ();
	XMLPropertyList const & qp = q->properties ();
	CPPUNIT_ASSERT_EQUAL (pp.size(), qp.size());

	XMLPropertyList::const_iterator i = pp.begin ();
	XMLPropertyList::const_iterator j = qp.begin ();
	while (i != pp.end ()) {
		CPPUNIT_ASSERT_EQUAL ((*i)->name(), (*j)->name());
		if (find (ignore_properties.begin(), ignore_properties.end(), (*i)->name ()) == ignore_properties.end ()) {
			CPPUNIT_ASSERT_EQUAL_MESSAGE ((*i)->name(), (*i)->value(), (*j)->value());
		}
		++i;
		++j;
	}

	XMLNodeList const & pc = p->children ();
	XMLNodeList const & qc = q->children ();

	CPPUNIT_ASSERT_EQUAL (pc.size(), qc.size());
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

class TestReceiver : public Receiver 
{
protected:
	void receive (Transmitter::Channel chn, const char * str) {
		const char *prefix = "";
		
		switch (chn) {
		case Transmitter::Error:
			prefix = ": [ERROR]: ";
			break;
		case Transmitter::Info:
			/* ignore */
			return;
		case Transmitter::Warning:
			prefix = ": [WARNING]: ";
			break;
		case Transmitter::Fatal:
			prefix = ": [FATAL]: ";
			break;
		case Transmitter::Throw:
			/* this isn't supposed to happen */
			abort ();
		}
		
		/* note: iostreams are already thread-safe: no external
		   lock required.
		*/
		
		cout << prefix << str << endl;
		
		if (chn == Transmitter::Fatal) {
			exit (9);
		}
	}
};

TestReceiver test_receiver;
static const char* localedir = LOCALEDIR;

/** @param dir Session directory.
 *  @param state Session state file, without .ardour suffix.
 */
Session *
load_session (string dir, string state)
{
	ARDOUR::init (false, true, localedir);
	SessionEvent::create_per_thread_pool ("test", 512);

	test_receiver.listen_to (error);
	test_receiver.listen_to (info);
	test_receiver.listen_to (fatal);
	test_receiver.listen_to (warning);

	/* We can't use VSTs here as we have a stub instead of the
	   required bits in gtk2_ardour.
	*/
	Config->set_use_lxvst (false);

	AudioEngine* engine = AudioEngine::create ();

	CPPUNIT_ASSERT (engine->set_backend ("Dummy", "", ""));

	init_post_engine ();

	CPPUNIT_ASSERT (engine->start () == 0);

	Session* session = new Session (*engine, dir, state);
	engine->set_session (session);
	return session;
}

PBD::Searchpath
test_search_path ()
{
#ifdef PLATFORM_WINDOWS
	std::string wsp(g_win32_get_package_installation_directory_of_module(NULL));
	return Glib::build_filename (wsp, "ardour_testdata");
#else
	return Glib::getenv("ARDOUR_TEST_PATH");
#endif
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
