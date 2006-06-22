
#include <ardour/session.h>
#include <control_protocol/control_protocol.h>
#include <generic_midi/generic_midi_control_protocol.h>
#include <transport/tranzport_control_protocol.h>

using namespace ARDOUR;

void
Session::initialize_control ()
{
	GenericMidiControlProtocol* midi_protocol = new GenericMidiControlProtocol (*this);

	if (midi_protocol->init() == 0) {
		control_protocols.push_back (midi_protocol);
	}

	if (Config->get_use_tranzport()) {
		cerr << "Creating new tranzport control" << endl;

		TranzportControlProtocol* tranzport_protocol = new TranzportControlProtocol (*this);

		cerr << "Initializing new tranzport control" << endl;

		if (tranzport_protocol->init() == 0) {
			control_protocols.push_back (tranzport_protocol);
		}
	}
}

