#include "SequenceTest.h"
#include <cassert>

CPPUNIT_TEST_SUITE_REGISTRATION(SequenceTest);

using namespace std;
using namespace Evoral;

void
SequenceTest::createTest ()
{
	CPPUNIT_ASSERT_EQUAL(size_t(0), seq->sysexes().size());
	CPPUNIT_ASSERT_EQUAL(size_t(0), seq->notes().size());
	CPPUNIT_ASSERT(seq->notes().begin() == seq->notes().end());
}

void
SequenceTest::copyTest ()
{
	DummyTypeMap map;
	MySequence<Time> a(map);
	for (Notes::const_iterator i = test_notes.begin(); i != test_notes.end(); ++i) {
		a.notes().insert(*i);
	}

	MySequence<Time> b(a);
	CPPUNIT_ASSERT_EQUAL(b.notes().size(), a.notes().size());
}

void
SequenceTest::preserveEventOrderingTest ()
{
	vector< boost::shared_ptr< Event<Time> > > inserted_events;

	seq->start_write();

	for (Notes::const_iterator i = test_notes.begin(); i != test_notes.end(); ++i) {
		uint8_t buffer[3];
		Event<Time>* event = new Event<Time>(
			(Evoral::EventType)DummyTypeMap::CONTROL, (*i)->on_event().time(), 3, buffer, true
		);

		event->buffer()[0] = MIDI_CMD_CONTROL;
		event->buffer()[1] = event->time().to_double() / 1000;
		event->buffer()[2] = event->time().to_double() / 1000;

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

	// Iterate over all notes
	bool on = true;
	for (Sequence<Time>::const_iterator i = seq->begin(Time(600)); i != seq->end(); ++i) {
		if (on) {
			CPPUNIT_ASSERT(i->is_note_on());
			CPPUNIT_ASSERT_EQUAL(i->time(), Time((num_notes + 6) * 100));
			++num_notes;
			on = false;
		} else {
			CPPUNIT_ASSERT(i->is_note_off());
			on = true;
		}
	}

	CPPUNIT_ASSERT_EQUAL(size_t(6), num_notes);

	// Test invalidation
	Sequence<Time>::const_iterator i = seq->begin(Time(600));
	std::set< boost::weak_ptr< Note<Time> > > active_notes;
	i.invalidate(&active_notes);
	CPPUNIT_ASSERT_EQUAL((size_t)1, active_notes.size());

	// Test resuming after invalidation
	i = seq->begin(Time(601), false, std::set<Evoral::Parameter>(), &active_notes);
	CPPUNIT_ASSERT(i->is_note_off());
	on = false;
	num_notes = 1;
	for (; i != seq->end(); ++i) {
		if (on) {
			CPPUNIT_ASSERT(i->is_note_on());
			CPPUNIT_ASSERT_EQUAL(Time((num_notes + 6) * 100), i->time());
			++num_notes;
			on = false;
		} else {
			CPPUNIT_ASSERT(i->is_note_off());
			on = true;
		}
	}

	CPPUNIT_ASSERT_EQUAL(size_t(6), num_notes);

	// Test equality of copied iterators
	i = seq->begin();
	++i;
	Sequence<Time>::const_iterator j = i;
	CPPUNIT_ASSERT(i == j);
}

void
SequenceTest::controlInterpolationTest ()
{
	seq->clear();

	static const uint64_t delay   = 1000;
	static const uint32_t cc_type = 1;

	boost::shared_ptr<Control> c = seq->control(Parameter(cc_type, 1, 1), true);
	CPPUNIT_ASSERT(c);

	double min = 0.0;
	double max = 127.0;

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
	CPPUNIT_ASSERT_EQUAL((size_t)3, sink.events.size());
	CPPUNIT_ASSERT_EQUAL(Time(0), sink.events[0].first);
	CPPUNIT_ASSERT_EQUAL((uint8_t)0, sink.events[0].second);
	CPPUNIT_ASSERT_EQUAL(Time(1000), sink.events[1].first);
	CPPUNIT_ASSERT_EQUAL((uint8_t)127, sink.events[1].second);
	CPPUNIT_ASSERT_EQUAL(Time(2000), sink.events[2].first);
	CPPUNIT_ASSERT_EQUAL((uint8_t)0, sink.events[2].second);
	sink.events.clear();
	CPPUNIT_ASSERT_EQUAL((size_t)0, sink.events.size());

	// Test linear interpolation
	c->list()->set_interpolation(ControlList::Linear);
	for (MySequence<Time>::const_iterator i = seq->begin(); i != seq->end(); ++i) {
		sink.write(i->time(), i->event_type(), i->size(), i->buffer());
	}
	CPPUNIT_ASSERT_EQUAL((size_t)(128 * 2 - 1), sink.events.size());
	Time    last_time(0);
	int16_t last_value = -1;
	bool    ascending  = true;
	for (CCTestSink<Time>::Events::const_iterator i = sink.events.begin();
			i != sink.events.end(); ++i) {
		CPPUNIT_ASSERT(last_time == 0 || i->first > last_time);
		if (last_value == 127) {
			ascending = false;
		}
		if (ascending) {
			CPPUNIT_ASSERT_EQUAL((int)i->second, last_value + 1);
		} else {
			CPPUNIT_ASSERT_EQUAL((int)i->second, last_value - 1);
		}
		last_time = i->first;
		last_value = i->second;
	}
}
