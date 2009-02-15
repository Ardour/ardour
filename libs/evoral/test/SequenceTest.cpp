#include "SequenceTest.hpp"
#include <cassert>

CPPUNIT_TEST_SUITE_REGISTRATION(SequenceTest);

using namespace std;

void 
SequenceTest::createTest (void)
{
	CPPUNIT_ASSERT_EQUAL(size_t(0), seq->sysexes().size());
	CPPUNIT_ASSERT_EQUAL(size_t(0), seq->notes().size());
	CPPUNIT_ASSERT(seq->notes().begin() == seq->notes().end());
}


void 
SequenceTest::preserveEventOrderingTest (void)
{
	vector< boost::shared_ptr< Event<Time> > > inserted_events;
	
	seq->start_write();
	
	for (Notes::const_iterator i = test_notes.begin(); i != test_notes.end(); ++i) {
		uint8_t buffer[2];
		Event<Time>* event = new Event<Time>(
				DummyTypeMap::CONTROL, (*i)->on_event().time(), 3, buffer, true
		);
		
		event->buffer()[0] = MIDI_CMD_CONTROL;
		event->buffer()[1] = event->time() / 100;
		event->buffer()[2] = event->time() / 100;
		
		boost::shared_ptr<Event<Time> > event_ptr(event);

		seq->append((*i)->on_event());
		inserted_events.push_back(
				boost::shared_ptr<Event<Time> >(
						new Event<Time>((*i)->on_event(), true)
		));
		
		seq->append(*event_ptr);
		inserted_events.push_back(event_ptr);
		
		seq->append((*i)->off_event());
		inserted_events.push_back(
				boost::shared_ptr<Event<Time> >(
						new Event<Time>((*i)->off_event(), true)
		));
	}
	
	seq->end_write();

	TestSink<Time> sink;
	sink.writing.connect(sigc::mem_fun(&sink, &TestSink<Time>::assertLastEventTimeEarlier));


	for (MySequence<Time>::const_iterator i = seq->begin(); i != seq->end(); ++i) {
		sink.write(i->time(), i->event_type(), i->size(), i->buffer());
	}
	
	CPPUNIT_ASSERT_EQUAL(size_t(12), test_notes.size());
}

