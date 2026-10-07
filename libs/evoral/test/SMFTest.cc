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

static void
read_smf_to_sequence (TestSMF& smf, MySequence<SMFTest::Time>& seq)
{
	/*
	 * Read an SMF file into a Sequence, including meta events.
	 *
	 * SMF::read_event() returns 0 for ALL meta events (they are
	 * considered "skipped" at the Evoral level).  To load them into
	 * the Sequence — as SMFSource::load_model_unlocked() does at the
	 * ARDOUR level — we must check the buffer for 0xFF status and
	 * explicitly append those events too.
	 */
	typedef SMFTest::Time Time;

	seq.start_write();
	smf.seek_to_start();

	uint64_t time = 0;
	Evoral::Event<Time> ev;

	uint32_t delta_t = 0;
	uint32_t size    = 0;
	uint8_t* buf     = NULL;
	int ret;
	while ((ret = smf.read_event(&delta_t, &size, &buf)) >= 0) {
		time += delta_t;

		if (ret > 0) {
			/* Normal MIDI event */
			ev.set(buf, size, Time());
			ev.set_time(Temporal::Beats::ticks_at_rate(time, smf.ppqn()));
			ev.set_event_type(Evoral::MIDI_EVENT);
			seq.append(ev, next_event_id ());
		} else if (ret == 0 && size >= 2 && buf[0] == 0xFF
		           && buf[1] >= 0x01 && buf[1] <= 0x09) {
			/* Text-type meta event (0x01..0x09).
			 * SMF::read_event() returns 0 for all meta events.
			 * Note: for 0x7F sequencer-specific events, read_event()
			 * returns 0 WITHOUT updating buf/size (stale data).
			 * The write step uses note_id=-1 to avoid writing 0x7F
			 * Evoral note IDs, which prevents stale buffer issues
			 * on re-read.
			 */
			ev.set(buf, size, Time());
			ev.set_time(Temporal::Beats::ticks_at_rate(time, smf.ppqn()));
			ev.set_event_type(Evoral::MIDI_EVENT);
			seq.append(ev, next_event_id ());
		}
	}

	seq.end_write (Sequence<Time>::Relax,
	               Temporal::Beats::ticks_at_rate(time, smf.ppqn()));
}

void
SMFTest::metaEventRoundTripTest ()
{
	/*
	 * MetaText.mid contains:
	 *   at tick 0: Track Name (0x03) "Test Track"
	 *   at tick 0: Text (0x01) "Hello World"
	 *   at tick 0: Copyright (0x02) "2024 Test"
	 *   at tick 0: Instrument (0x04) "Piano"
	 *   at tick 0: Lyric (0x05) "La"
	 *   at tick 0: Marker (0x06) "Intro"
	 *   at tick 0: Cue Point (0x07) "Cue1"
	 *   at tick 0: Tempo (0x51) — not a text meta, not loaded here
	 *   at tick 0: Time Sig (0x58) — not a text meta, not loaded here
	 *   at tick 0: Note On C4
	 *   at tick 480: Note Off C4
	 *   at tick 480: Text (0x01) "Mid text"
	 *   at tick 480: Note On E4
	 *   at tick 960: Note Off E4
	 *   at tick 1440: Lyric (0x05) "La la"
	 *   End of track
	 *
	 * Text meta events 0x01-0x07 at tick 0 (7), plus 0x01 at tick 480 (1),
	 * plus 0x05 at tick 1440 (1) = 9 text-type meta events total.
	 */

	/* Step 1: Read MetaText.mid into a Sequence */
	TestSMF smf;
	string testdata_path;
	CPPUNIT_ASSERT (find_file (test_search_path (), "MetaText.mid", testdata_path));
	CPPUNIT_ASSERT (SMF::test(testdata_path));

	smf.open(testdata_path);
	CPPUNIT_ASSERT(!smf.is_empty());
	CPPUNIT_ASSERT_EQUAL((uint16_t)1, smf.num_tracks());
	CPPUNIT_ASSERT_EQUAL(0, smf.seek_to_track(1));

	read_smf_to_sequence(smf, *seq);
	CPPUNIT_ASSERT(!seq->empty());

	/* Verify notes loaded */
	CPPUNIT_ASSERT_EQUAL(size_t(2), seq->notes().size());

	/* Verify 9 text-type meta events loaded (types 0x01-0x07, 0x01, 0x05) */
	CPPUNIT_ASSERT_EQUAL(size_t(9), seq->meta_events().size());

	/* Step 2: Iterate and count meta events through const_iterator */
	size_t num_notes = 0;
	size_t num_meta  = 0;
	for (Sequence<Time>::const_iterator i = seq->begin(Time()); i != seq->end(); ++i) {
		if (i->is_note_on()) {
			++num_notes;
		} else if (i->is_smf_meta_event()) {
			++num_meta;
		}
	}
	CPPUNIT_ASSERT_EQUAL(size_t(2), num_notes);
	CPPUNIT_ASSERT_EQUAL(size_t(9), num_meta);

	/* Step 3: Write the Sequence to a new SMF file via the iterator */
	TestSMF out;
	const string output_dir_path = PBD::tmp_writable_directory (PACKAGE, "metaEventRoundTripTest");
	const string new_file_path   = Glib::build_filename (output_dir_path, "MetaTextCopy.mid");
	CPPUNIT_ASSERT_EQUAL(0, out.create(new_file_path, 1, smf.ppqn()));
	out.begin_write();

	uint64_t last_ticks = 0;
	for (Sequence<Time>::const_iterator i = seq->begin(Time()); i != seq->end(); ++i) {
		uint64_t ticks = i->time().to_ticks(smf.ppqn());
		uint32_t dt = (uint32_t)(ticks - last_ticks);
		last_ticks = ticks;
		/* note_id = -1: do not write Evoral note IDs (0x7F meta events)
		 * since we are testing text meta event round-trip only.
		 */
		out.append_event_delta(dt, i->size(), i->buffer(), -1, i->is_smf_meta_event());
	}

	out.end_write(new_file_path);
	smf.close();

	/* Step 4: Re-read the written file into a fresh Sequence */
	MySequence<Time> seq2(*type_map);

	TestSMF smf2;
	smf2.open(new_file_path);
	CPPUNIT_ASSERT(!smf2.is_empty());
	CPPUNIT_ASSERT_EQUAL(0, smf2.seek_to_track(1));

	read_smf_to_sequence(smf2, seq2);

	/* Verify round-trip: same number of notes and meta events */
	CPPUNIT_ASSERT_EQUAL(size_t(2), seq2.notes().size());
	CPPUNIT_ASSERT_EQUAL(size_t(9), seq2.meta_events().size());

	/* Verify meta events come out in correct time order through iterator */
	num_notes = 0;
	num_meta  = 0;
	for (Sequence<Time>::const_iterator i = seq2.begin(Time()); i != seq2.end(); ++i) {
		if (i->is_note_on()) {
			++num_notes;
		} else if (i->is_smf_meta_event()) {
			++num_meta;
		}
	}
	CPPUNIT_ASSERT_EQUAL(size_t(2), num_notes);
	CPPUNIT_ASSERT_EQUAL(size_t(9), num_meta);

	smf2.close();
}
