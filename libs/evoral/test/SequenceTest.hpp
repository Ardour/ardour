#include <cassert>
#include <sigc++/sigc++.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "evoral/Sequence.hpp"
#include "evoral/TypeMap.hpp"
#include "evoral/EventSink.hpp"
#include "evoral/midi_events.h"
#include "evoral/Control.hpp"

using namespace Evoral;

class DummyTypeMap : public TypeMap {
public:
	enum DummyEventType {
		NOTE,
		CONTROL,
		SYSEX
	};

	~DummyTypeMap() {}

	bool type_is_midi (uint32_t /*type*/) const { return true; }

	uint8_t parameter_midi_type(const Parameter& param) const {
		switch (param.type()) {
		case CONTROL:       return MIDI_CMD_CONTROL;
		case SYSEX:         return MIDI_CMD_COMMON_SYSEX;
		default:            return 0;
		};
	}

	uint32_t midi_event_type(uint8_t status) const {
		status &= 0xf0;
		switch (status) {
		case MIDI_CMD_CONTROL:          return CONTROL;
		case MIDI_CMD_COMMON_SYSEX:     return SYSEX;
		default:                        return 0;
		};
	}

	bool is_integer (const Evoral::Parameter& /*param*/) const { return true; }

	Parameter new_parameter(uint32_t type, uint8_t channel, uint32_t id) const {
		Parameter p(type, channel, id);
		p.set_range(type, 0.0f, 1.0f, 0.0f);
		return p;
	}

	std::string to_symbol(const Parameter& /*param*/) const { return "control"; }
};

template<typename Time>
class MySequence : public Sequence<Time> {
public:
	MySequence(DummyTypeMap&map) : Sequence<Time>(map) {}

	boost::shared_ptr<Control> control_factory(const Parameter& param) {

		return boost::shared_ptr<Control>(
			new Control(param, boost::shared_ptr<ControlList> (
				new ControlList(param)
		)));
	}
};

template<typename Time>
class TestSink : public EventSink<Time> {
public:
	TestSink() : _last_event_time(-1) {}

	/// return value, time, type, size, buffer
	sigc::signal<uint32_t, Time, EventType, uint32_t, const uint8_t*> writing;

	virtual uint32_t write(Time time, EventType type, uint32_t size, const uint8_t* buf) {
		//std::cerr << "last event time: " << _last_event_time << " time: " << time << std::endl;
		uint32_t result = writing(time, type, size, buf);
		_last_event_time = time;
		return result;
	}

	uint32_t assertLastEventTimeEarlier(
			Time time, EventType /*type*/, uint32_t /*size*/, const uint8_t* /*buf*/) {
		CPPUNIT_ASSERT(_last_event_time <= time);
		return 0;
	}

	Time last_event_time() const { return _last_event_time; }

private:
	Time _last_event_time;
};

template<typename Time>
class CCTestSink : public EventSink<Time> {
public:
	CCTestSink(uint32_t t) : cc_type(t) {}

	virtual uint32_t write(Time time, EventType type, uint32_t size, const uint8_t* buf) {
		if (type == cc_type) {
			CPPUNIT_ASSERT(size == 3);
			events.push_back(std::make_pair(time, buf[2]));
		}
		return size;
	}

	typedef std::vector< std::pair<Time, uint8_t> > Events;
	Events events;
	uint32_t cc_type;
};

class SequenceTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (SequenceTest);
	CPPUNIT_TEST (createTest);
	CPPUNIT_TEST (preserveEventOrderingTest);
	CPPUNIT_TEST (iteratorSeekTest);
	CPPUNIT_TEST (controlInterpolationTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	typedef double Time;
	typedef std::vector< boost::shared_ptr< Note<Time> > > Notes;

	void setUp () {
		type_map = new DummyTypeMap();
		assert(type_map);
		seq = new MySequence<Time>(*type_map);
		assert(seq);

		for (int i = 0; i < 12; i++) {
			test_notes.push_back(boost::shared_ptr<Note<Time> >
					(new Note<Time>(0, i * 100, 100, 64 + i, 64)));
		}
	}

	void tearDown () {
		test_notes.clear();
		delete seq;
		delete type_map;
	}

	void createTest ();
	void preserveEventOrderingTest ();
	void iteratorSeekTest ();
	void controlInterpolationTest ();

private:
	DummyTypeMap*       type_map;
	MySequence<Time>*   seq;

	Notes test_notes;
};
