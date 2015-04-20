#include "SMFTest.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/file_utils.h"

using namespace std;

CPPUNIT_TEST_SUITE_REGISTRATION( SMFTest );

void
SMFTest::createNewFileTest ()
{
	TestSMF smf;

	string output_dir_path = PBD::tmp_writable_directory (PACKAGE, "createNewFileTest");
	string new_file_path = Glib::build_filename (output_dir_path, "NewFile.mid");
	smf.create(new_file_path);
	smf.close();
	CPPUNIT_ASSERT(Glib::file_test (new_file_path, Glib::FILE_TEST_IS_REGULAR));
}

PBD::Searchpath
test_search_path ()
{
#ifdef PLATFORM_WINDOWS
	string wsp(g_win32_get_package_installation_directory_of_module(NULL));
	return Glib::build_filename (wsp,  "evoral_testdata");
#else
	return Glib::getenv("EVORAL_TEST_PATH");
#endif
}

void
SMFTest::takeFiveTest ()
{
	TestSMF smf;
	string testdata_path;
	CPPUNIT_ASSERT (find_file (test_search_path (), "TakeFive.mid", testdata_path));
	smf.open(testdata_path);
	CPPUNIT_ASSERT(!smf.is_empty());

	seq->start_write();
	smf.seek_to_start();

	uint64_t time = 0; /* in SMF ticks */
	Evoral::Event<Evoral::Beats> ev;

	uint32_t delta_t = 0;
	uint32_t size    = 0;
	uint8_t* buf     = NULL;
	int ret;
	while ((ret = smf.read_event(&delta_t, &size, &buf)) >= 0) {
		ev.set(buf, size, Evoral::Beats());
		time += delta_t;

		if (ret > 0) { // didn't skip (meta) event
			//cerr << "read smf event type " << hex << int(buf[0]) << endl;
			ev.set_time(Evoral::Beats::ticks_at_rate(time, smf.ppqn()));
			ev.set_event_type(type_map->midi_event_type(buf[0]));
			seq->append(ev, next_event_id ());
		}
	}

	seq->end_write (Sequence<Time>::Relax,
	                Evoral::Beats::ticks_at_rate(time, smf.ppqn()));
	CPPUNIT_ASSERT(!seq->empty());
}
