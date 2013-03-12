#include "SequenceTest.hpp"
#include "evoral/MIDIParameters.hpp"
#include <cassert>

CPPUNIT_TEST_SUITE_REGISTRATION(SequenceTest);

using namespace std;

void
SequenceTest::createTest ()
{
	CPPUNIT_ASSERT_EQUAL(size_t(0), seq->sysexes().size());
	CPPUNIT_ASSERT_EQUAL(size_t(0), seq->notes().size());
	CPPUNIT_ASSERT(seq->notes().begin() == seq->notes().end());
}


void
SequenceTest::preserveEventOrderingTest ()
{
	vector< boost::shared_ptr< Event<Time> > > inserted_events;

	seq->start_write();

	for (Notes::const_iterator i = test_notes.begin(); i != test_notes.end(); ++i) {
		uint8_t buffer[2];
		Event<Time>* event = new Event<Time>(
				DummyTypeMap::CONTROL, (*i)->on_event().time(), 3, buffer, true
		);

		event->buffer()[0] = MIDI_CMD_CONTROL;
		event->buffer()[1] = event->time() / 1000;
		event->buffer()[2] = event->time() / 1000;

		boost::shared_ptr<Event<Time> > event_ptr(event);

		seq->append((*i)->on_event(), next_event_id ());
		inserted_events.push_back(
				boost::shared_ptr<Event<Time> >(
						new Event<Time>((*i)->on_event(), true)
		));

		seq->append(*event_ptr, next_event_id ());
		inserted_events.push_back(event_ptr);

		seq->append((*i)->off_event(), next_event_id ());
		inserted_events.push_back(
				boost::shared_ptr<Event<Time> >(
						new Event<Time>((*i)->off_event(), true)
		));
	}

	seq->end_write (Sequence<Time>::Relax);

	TestSink<Time> sink;
	sink.writing.connect(sigc::mem_fun(&sink, &TestSink<Time>::assertLastEventTimeEarlier));


	for (MySequence<Time>::const_iterator i = seq->begin(); i != seq->end(); ++i) {
		sink.write(i->time(), i->event_type(), i->size(), i->buffer());
	}

	CPPUNIT_ASSERT_EQUAL(size_t(12), test_notes.size());
}

void
SequenceTest::iteratorSeekTest ()
{
	size_t num_notes = 0;

	seq->clear();

	for (Notes::const_iterator i = test_notes.begin(); i != test_notes.end(); ++i) {
		seq->notes().insert(*i);
	}

	bool on = true;
	for (Sequence<Time>::const_iterator i = seq->begin(600); i != seq->end(); ++i) {
		if (on) {
			CPPUNIT_ASSERT(((const MIDIEvent<Time>&)*i).is_note_on());
			CPPUNIT_ASSERT_EQUAL(i->time(), Time((num_notes + 6) * 100));
			++num_notes;
			on = false;
		} else {
			CPPUNIT_ASSERT(((const MIDIEvent<Time>&)*i).is_note_off());
			on = true;
		}
	}

	CPPUNIT_ASSERT_EQUAL(num_notes, size_t(6));
}

void
SequenceTest::controlInterpolationTest ()
{
	seq->clear();

	for (Notes::const_iterator i = test_notes.begin(); i != test_notes.end(); ++i) {
		seq->notes().insert(*i);
	}

	static const uint64_t delay   = 1000;
	static const uint32_t cc_type = 1;

	boost::shared_ptr<Control> c = seq->control(MIDI::ContinuousController(cc_type, 1, 1), true);
	CPPUNIT_ASSERT(c);

	double min, max, normal;
	MIDI::controller_range(min, max, normal);

	// Make a ramp like /\ from min to max and back to min
	c->set_double(min, 0, true);
	c->set_double(max, delay, true);
	c->set_double(min, 2*delay, true);

	CCTestSink<Time> sink(cc_type);

	// Test discrete (lack of) interpolation
	c->list()->set_interpolation(ControlList::Discrete);
	for (MySequence<Time>::const_iterator i = seq->begin(); i != seq->end(); ++i) {
		sink.write(i->time(), i->event_type(), i->size(), i->buffer());
	}
	CPPUNIT_ASSERT(sink.events.size() == 3);
	CPPUNIT_ASSERT(sink.events[0].first == 0);
	CPPUNIT_ASSERT(sink.events[0].second == 0);
	CPPUNIT_ASSERT(sink.events[1].first == 1000);
	CPPUNIT_ASSERT(sink.events[1].second == 127);
	CPPUNIT_ASSERT(sink.events[2].first == 2000);
	CPPUNIT_ASSERT(sink.events[2].second == 0);
	sink.events.clear();
	CPPUNIT_ASSERT(sink.events.size() == 0);

	// Test linear interpolation
	c->list()->set_interpolation(ControlList::Linear);
	for (MySequence<Time>::const_iterator i = seq->begin(); i != seq->end(); ++i) {
		sink.write(i->time(), i->event_type(), i->size(), i->buffer());
	}
	CPPUNIT_ASSERT(sink.events.size() == 128 * 2 - 1);
	Time    last_time  = 0;
	int16_t last_value = -1;
	bool    ascending  = true;
	for (CCTestSink<Time>::Events::const_iterator i = sink.events.begin();
			i != sink.events.end(); ++i) {
		CPPUNIT_ASSERT(last_time == 0 || i->first > last_time);
		if (last_value == 127) {
			ascending = false;
		}
		if (ascending) {
			CPPUNIT_ASSERT(i->second == last_value + 1);
		} else {
			CPPUNIT_ASSERT(i->second == last_value - 1);
		}
		last_time = i->first;
		last_value = i->second;
	}
}
