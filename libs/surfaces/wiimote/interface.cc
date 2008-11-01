#include <pbd/failed_constructor.h>

#include <control_protocol/control_protocol.h>
#include "wiimote.h"

#include <ardour/session.h>

using namespace ARDOUR;

static WiimoteControlProtocol *foo;

ControlProtocol*
new_wiimote_protocol (ControlProtocolDescriptor* descriptor, Session* s)
{
	WiimoteControlProtocol* wmcp;
		
	try {
		wmcp =  new WiimoteControlProtocol (*s);
	} catch (failed_constructor& err) {
		return 0;
	}
	
	if (wmcp-> set_active (true)) {
		delete wmcp;
		return 0;
	}

	foo = wmcp;

	return wmcp;
}

void
wiimote_control_protocol_cwiid_callback(cwiid_wiimote_t *wiimote, int mesg_count, union cwiid_mesg mesg[], struct timespec *t)
{
	assert(foo != 0);

	foo->wiimote_callback(wiimote,mesg_count,mesg,t);
}

void
delete_wiimote_protocol (ControlProtocolDescriptor* descriptor, ControlProtocol* cp)
{
	foo = 0;
	delete cp;
}

bool
probe_wiimote_protocol (ControlProtocolDescriptor* descriptor)
{
	return WiimoteControlProtocol::probe ();
}

static ControlProtocolDescriptor wiimote_descriptor = {
	name : "Wiimote",
	id : "uri://ardour.org/surfaces/wiimote:0",
	ptr : 0,
	module : 0,
	mandatory : 0,
	supports_feedback : false,
	probe : probe_wiimote_protocol,
	initialize : new_wiimote_protocol,
	destroy : delete_wiimote_protocol
};
	

extern "C" {
ControlProtocolDescriptor* 
protocol_descriptor () {
	return &wiimote_descriptor;
}
}

