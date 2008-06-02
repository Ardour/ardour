#include "midi++/event.h"

namespace MIDI {

#ifdef MIDI_EVENT_ALLOW_ALLOC
Event::Event(double t, uint32_t s, uint8_t* b, bool owns_buffer)
	: _time(t)
	, _size(s)
	, _buffer(b)
	, _owns_buffer(owns_buffer)
{
	if (owns_buffer) {
		_buffer = (uint8_t*)malloc(_size);
		if (b) {
			memcpy(_buffer, b, _size);
		} else {
			memset(_buffer, 0, _size);
		}
	}
}

Event::Event(const XMLNode& event)
{
	string name = event.name();
	
	if (name == "ControlChange") {
		
	} else if (name == "ProgramChange") {
		
	}
}

Event::Event(const Event& copy, bool owns_buffer)
	: _time(copy._time)
	, _size(copy._size)
	, _buffer(copy._buffer)
	, _owns_buffer(owns_buffer)
{
	if (owns_buffer) {
		_buffer = (uint8_t*)malloc(_size);
		if (copy._buffer) {
			memcpy(_buffer, copy._buffer, _size);
		} else {
			memset(_buffer, 0, _size);
		}
	}
}

Event::~Event() {
	if (_owns_buffer) {
		free(_buffer);
	}
}


#endif // MIDI_EVENT_ALLOW_ALLOC

std::string      
Event::to_string() const 
{
	std::ostringstream result(std::ios::ate);
	result << "MIDI::Event type:" << std::hex << "0x" << int(type()) << "   buffer: ";
	
	for(uint32_t i = 0; i < size(); ++i) {
		result << " 0x" << int(_buffer[i]); 
	}
	return result.str();
}

boost::shared_ptr<XMLNode> 
Event::to_xml() const
{
	XMLNode *result = 0;
	
	switch (type()) {
		case MIDI_CMD_CONTROL:
			result = new XMLNode("ControlChange");
			result->add_property("Channel", channel());
			result->add_property("Control", cc_number());
			result->add_property("Value",   cc_value());
			break;
			
		case MIDI_CMD_PGM_CHANGE:
			result = new XMLNode("ProgramChange");
			result->add_property("Channel", channel());
			result->add_property("number",  pgm_number());
			break;
		
		default:
			// The implementation is continued as needed
			break;
	}
	
	return boost::shared_ptr<XMLNode>(result);
}

} // namespace MIDI
