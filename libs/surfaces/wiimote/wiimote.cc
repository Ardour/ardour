#include "wiimote.h"

#include <iostream>
#include <sigc++/bind.h>

#include <pbd/xml++.h>
#include <ardour/session.h>

#include "i18n.h"


using namespace ARDOUR;
using namespace PBD;

void wiimote_control_protocol_cwiid_callback(cwiid_wiimote_t *wiimote, int mesg_count, union cwiid_mesg mesg[], struct timespec *t);

uint16_t WiimoteControlProtocol::button_state = 0;

WiimoteControlProtocol::WiimoteControlProtocol ( Session & session) 
	: ControlProtocol ( session, "Wiimote"),
	  thread_quit (false),
	  thread_registered_for_ardour (false)
{
	std::cerr << "WiimoteControlProtocol()" << std::endl;
	thread = Glib::Thread::create( sigc::mem_fun(*this, &WiimoteControlProtocol::main_thread), true);
}

WiimoteControlProtocol::~WiimoteControlProtocol()
{
	thread_quit = true;
	thread->join();
	// TODO: you con't delete the thread, but is join() still enough?
	std::cerr << "~WiimoteControlProtocol()" << std::endl;
}


bool 
WiimoteControlProtocol::probe()
{
	return true;
}

void
WiimoteControlProtocol::wiimote_callback(cwiid_wiimote_t *wiimote, int mesg_count, union cwiid_mesg mesg[], struct timespec *t)
{
	int i;
	uint16_t b;

	if (!thread_registered_for_ardour) {
		register_thread("Wiimote Control Protocol");
		thread_registered_for_ardour = true;
	}

        for (i=0; i < mesg_count; i++)
	{
		if (mesg[i].type != CWIID_MESG_BTN) continue;

		b = (mesg[i].btn_mesg.buttons ^ button_state) & mesg[i].btn_mesg.buttons;

		button_state = mesg[i].btn_mesg.buttons;

		if (b & CWIID_BTN_2) {
			rec_enable_toggle();
			//std::cerr << "2" << std::endl;
		}
		if (b & CWIID_BTN_1) {
			access_action("Editor/track-record-enable-toggle");			
			//std::cerr << "1" << std::endl;
		}
		if (b & CWIID_BTN_B) {
			// just a B doesn't do anything
			//std::cerr << "B" << std::endl;
		}
		if (b & CWIID_BTN_A && button_state & CWIID_BTN_B) {
			// B pressed down and then A
			access_action("Transport/ToggleRollForgetCapture");
			//std::cerr << "B+A" << std::endl;
		}

		if (b & CWIID_BTN_A && !(button_state & CWIID_BTN_B)) {
			// Just A pressed
			access_action("Transport/ToggleRoll");
			//std::cerr << "A" << std::endl;
		}
		if (b & CWIID_BTN_MINUS) {
			access_action("Editor/temporal-zoom-out");
			//std::cerr << "-" << std::endl;
		}
		if (b & CWIID_BTN_HOME) {
			//std::cerr << "home" << std::endl;
		}
		if (b & CWIID_BTN_LEFT) {
			access_action("Editor/nudge-playhead-backward");
			//std::cerr << "<" << std::endl;
		}
		if (b & CWIID_BTN_RIGHT) {
			access_action("Editor/nudge-playhead-forward");
			//std::cerr << ">" << std::endl;
		}
		if (b & CWIID_BTN_DOWN) {
			access_action("Editor/select-next-route");
			//std::cerr << "_" << std::endl;
		}
		if (b & CWIID_BTN_UP) {
			access_action("Editor/select-prev-route");
			//std::cerr << "^" << std::endl;
		}
		if (b & CWIID_BTN_PLUS) {
			access_action("Editor/temporal-zoom-in");
			//std::cerr << "+" << std::endl;
		}

	}
}

void
WiimoteControlProtocol::main_thread()
{
	cwiid_wiimote_t *wiimote_handle = 0;
	bdaddr_t bdaddr;
	unsigned char rpt_mode = 0;

	std::cerr << "wiimote: discovering, press 1+2" << std::endl;

 	while (!wiimote_handle && !thread_quit) {
		bdaddr = *BDADDR_ANY;
		wiimote_handle = cwiid_open(&bdaddr, 0);

		if (!wiimote_handle) {
			sleep(1); 
			// We don't know whether the issue was a timeout or a configuration 
			// issue
		}
	}

	if (thread_quit) {
		// The corner case where the wiimote is bound at the same time as
		// the control protocol is destroyed
		if (wiimote_handle) {
			cwiid_close(wiimote_handle);
		}
		std::cerr << "Wiimote Control Protocol stopped before connected to a wiimote" << std::endl;
		return;
	}

	std::cerr << "Wiimote: connected" << std::endl;
	WiimoteControlProtocol::button_state = 0;

	if (cwiid_enable(wiimote_handle, CWIID_FLAG_REPEAT_BTN)) {
		std::cerr << "cwiid_enable(), error" << std::endl;
		cwiid_close(wiimote_handle);
		return;
	}
	if (cwiid_set_mesg_callback(wiimote_handle, wiimote_control_protocol_cwiid_callback)) {
		std::cerr << "cwiid_set_mesg_callback(), couldn't connect callback" << std::endl;
		cwiid_close(wiimote_handle);
		return;
	} 
	if (cwiid_command(wiimote_handle, CWIID_CMD_RPT_MODE, CWIID_RPT_BTN)) {
		std::cerr << "cwiid_command(), RPT_MODE error" << std::endl;
		cwiid_close(wiimote_handle);
		return;
	}

	rpt_mode |= CWIID_RPT_BTN;
	cwiid_enable(wiimote_handle, CWIID_FLAG_MESG_IFC);
	cwiid_set_rpt_mode(wiimote_handle, rpt_mode);
	
	while (!thread_quit) {
		sleep(1);
	}	

	cwiid_close(wiimote_handle);
	std::cerr << "Wiimote: stopped" << std::endl;
}


int
WiimoteControlProtocol::set_active (bool yn)
{
	// Let's not care about this just yet
	return 0;

}

XMLNode&
WiimoteControlProtocol::get_state()
{
	XMLNode *node = new XMLNode ("Protocol");
        node->add_property (X_("name"), _name);
        node->add_property (X_("feedback"), "0");

	return *node;
}

int
WiimoteControlProtocol::set_state(const XMLNode& node)
{
	return 0;
}
