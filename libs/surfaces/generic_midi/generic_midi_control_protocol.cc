#include <midi++/port.h>

#include <ardour/route.h>
#include <ardour/session.h>

#include "generic_midi_control_protocol.h"

using namespace ARDOUR;

#include "i18n.h"

GenericMidiControlProtocol::GenericMidiControlProtocol (Session& s)
	: ControlProtocol  (s, _("GenericMIDI"))
{
	_port = s.midi_port();
	s.MIDI_PortChanged.connect (mem_fun (*this, &GenericMidiControlProtocol::port_change));
	
}

GenericMidiControlProtocol::~GenericMidiControlProtocol ()
{
}

int
GenericMidiControlProtocol::set_active (bool yn)
{
	/* start delivery/outbound thread */
	return 0;
}

void
GenericMidiControlProtocol::port_change ()
{
	_port = session->midi_port ();
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
		MIDI::byte buf[bufsize];
		int32_t bsize = bufsize;
		MIDI::byte* end = buf;
		
		for (list<Route*>::iterator r = routes.begin(); r != routes.end(); ++r) {
			end = (*r)->write_midi_feedback (end, bsize);
		}
		
		if (end == buf) {
			return;
		} 
		
		_port->write (buf, 0, (int32_t) (end - buf));
		//cerr << "MIDI feedback: wrote " << (int32_t) (end - buf) << " to midi port\n";
	}
}

