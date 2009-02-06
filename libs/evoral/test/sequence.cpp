#include <evoral/Sequence.hpp>
#include <evoral/TypeMap.hpp>

using namespace Evoral;

class DummyTypeMap : public TypeMap {
public:
	virtual ~DummyTypeMap() {}

	virtual bool type_is_midi(uint32_t type) const {return true;}

	virtual uint8_t parameter_midi_type(const Parameter& param) const {return 0;}
	
	virtual uint32_t midi_event_type(uint8_t status) const {return 0;}
	
	virtual bool is_integer(const Evoral::Parameter& param) const {return true;}

	virtual Parameter new_parameter(uint32_t type, uint8_t channel, uint32_t id) const {return Parameter(type, channel, id);}

	virtual std::string to_symbol(const Parameter& param) const {return "foo";}
};

template<typename Time>
class MySequence : public Sequence<Time> {
public:
	MySequence(DummyTypeMap&map, int size) : Sequence<Time>(map, size) {}

	boost::shared_ptr<Control> control_factory(const Parameter& param) {return boost::shared_ptr<Control>();}
};


int
main()
{
	Glib::thread_init();
	DummyTypeMap type_map;

	MySequence<double> s(type_map, 100);
	return 0;
}
