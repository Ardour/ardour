#include <ardour/generic_midi_control_protocol.h>
#include <ardour/route.h>
#include <ardour/session.h>

using namespace ARDOUR;

#include "i18n.h"

GenericMidiControlProtocol::GenericMidiControlProtocol (Session& s)
	: ControlProtocol  (s, _("GenericMIDI"))
{
	_port = 0;
}

GenericMidiControlProtocol::~GenericMidiControlProtocol ()
{
}

void
GenericMidiControlProtocol::set_port (MIDI::Port* p)
{
	_port = p;
}

void 
GenericMidiControlProtocol::send_route_feedback (list<Route*>& routes)
{
	if (_port != 0) {

		const int32_t bufsize = 16 * 1024;
		int32_t bsize = bufsize;
		MIDI::byte* buf = new MIDI::byte[bufsize];
		MIDI::byte* end = buf;
		
		for (list<Route*>::iterator r = routes.begin(); r != routes.end(); ++r) {
		end = (*r)->write_midi_feedback (end, bsize);
		}
		
		if (end == buf) {
			delete [] buf;
			return;
		} 
		
		session.deliver_midi (_port, buf, (int32_t) (end - buf));
		//cerr << "MIDI feedback: wrote " << (int32_t) (end - buf) << " to midi port\n";
	}
}

bool
GenericMidiControlProtocol::active() const
{
	return _port && send();
}

