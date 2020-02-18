#include "SMFTest.h"

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
	if (!getenv("EVORAL_TEST_PATH")) {
		string wsp(g_win32_get_package_installation_directory_of_module(NULL));
		return Glib::build_filename (wsp,  "evoral_testdata");
	}
#endif
	return Glib::getenv("EVORAL_TEST_PATH");
}

void
SMFTest::takeFiveTest ()
{
	TestSMF smf;
	string testdata_path;
	CPPUNIT_ASSERT (find_file (test_search_path (), "TakeFive.mid", testdata_path));
	CPPUNIT_ASSERT (SMF::test(testdata_path));

	smf.open(testdata_path);
	CPPUNIT_ASSERT(!smf.is_empty());

	CPPUNIT_ASSERT_EQUAL((uint16_t)1, smf.num_tracks());
	CPPUNIT_ASSERT_EQUAL(0, smf.seek_to_track(1));

	seq->start_write();
	smf.seek_to_start();

	uint64_t time = 0; /* in SMF ticks */
	Evoral::Event<Time> ev;

	uint32_t delta_t = 0;
	uint32_t size    = 0;
	uint8_t* buf     = NULL;
	int ret;
	while ((ret = smf.read_event(&delta_t, &size, &buf)) >= 0) {
		ev.set(buf, size, Time());
		time += delta_t;

		if (ret > 0) { // didn't skip (meta) event
			//cerr << "read smf event type " << hex << int(buf[0]) << endl;
			ev.set_time(Temporal::Beats::ticks_at_rate(time, smf.ppqn()));
			ev.set_event_type(Evoral::MIDI_EVENT);
			seq->append(ev, next_event_id ());
		}
	}

	seq->end_write (Sequence<Time>::Relax,
	                Temporal::Beats::ticks_at_rate(time, smf.ppqn()));
	CPPUNIT_ASSERT(!seq->empty());

	// Iterate over all notes
	bool   on          = true;
	size_t num_notes   = 0;
	size_t num_sysexes = 0;
	for (Sequence<Time>::const_iterator i = seq->begin(Time()); i != seq->end(); ++i) {
		if (i->is_note_on()) {
			++num_notes;
		} else if (i->is_sysex()) {
			++num_sysexes;
		}
	}
	CPPUNIT_ASSERT_EQUAL(size_t(3833), seq->notes().size());
	CPPUNIT_ASSERT_EQUAL(size_t(3833), num_notes);
	CPPUNIT_ASSERT_EQUAL(size_t(232), seq->sysexes().size());
	CPPUNIT_ASSERT_EQUAL(size_t(232), num_sysexes);
}

void
SMFTest::writeTest ()
{
	TestSMF smf;
	string  testdata_path;
	CPPUNIT_ASSERT (find_file (test_search_path (), "TakeFive.mid", testdata_path));

	smf.open(testdata_path);
	CPPUNIT_ASSERT(!smf.is_empty());

	TestSMF out;
	const string output_dir_path = PBD::tmp_writable_directory (PACKAGE, "writeTest");
	const string new_file_path   = Glib::build_filename (output_dir_path, "TakeFiveCopy.mid");
	CPPUNIT_ASSERT_EQUAL (0, out.create(new_file_path, 1, 1920));
	out.begin_write();

	uint32_t delta_t = 0;
	uint32_t size    = 0;
	uint8_t* buf     = NULL;
	while (smf.read_event(&delta_t, &size, &buf) >= 0) {
		out.append_event_delta(delta_t, size, buf, 0);
	}

	out.end_write(new_file_path);

	// TODO: Check files are actually equivalent
}
