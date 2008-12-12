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
	  main_thread_quit (false),
	  restart_discovery (false),
	  callback_thread_registered_for_ardour (false),
	  wiimote_handle (0)
{
	main_thread = Glib::Thread::create( sigc::mem_fun(*this, &WiimoteControlProtocol::wiimote_main), true);
}

WiimoteControlProtocol::~WiimoteControlProtocol()
{
	main_thread_quit = true;
	slot_cond.signal();
	main_thread->join();

	if (wiimote_handle) {
		cwiid_close(wiimote_handle);
	}
	std::cerr << "Wiimote: closed" << std::endl;
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

	if (!callback_thread_registered_for_ardour) {
		register_thread("Wiimote Control Protocol");
		callback_thread_registered_for_ardour = true;
	}

        for (i=0; i < mesg_count; i++)
	{
		if (mesg[i].type == CWIID_MESG_ERROR) {	
			std::cerr << "Wiimote: disconnect" << std::endl;
			restart_discovery = true;
			slot_cond.signal();
			return;
		}

		if (mesg[i].type != CWIID_MESG_BTN) continue;

		// what buttons are pressed down which weren't pressed down last time
		b = (mesg[i].btn_mesg.buttons ^ button_state) & mesg[i].btn_mesg.buttons;

		button_state = mesg[i].btn_mesg.buttons;
	
		// if B is pressed down
		if (button_state & CWIID_BTN_B) {
			if (b & CWIID_BTN_A) { // B is down and A is pressed
				access_action("Transport/ToggleRollForgetCapture");
			}

			if (b & CWIID_BTN_LEFT) {
				access_action("Editor/playhead-to-previous-region-boundary");
			}
			if (b & CWIID_BTN_RIGHT) {
				access_action("Editor/playhead-to-next-region-boundary");
			}
			if (b & CWIID_BTN_UP) {
				next_marker();
			}
			if (b & CWIID_BTN_DOWN) {
				prev_marker();
			}

			if (b & CWIID_BTN_HOME) {
				access_action("Editor/add-location-from-playhead");
			}

			if (b & CWIID_BTN_MINUS) {	
				access_action("Transport/GotoStart");
			}
			
			if (b & CWIID_BTN_PLUS) {
				access_action("Transport/GotoEnd");
			}

			continue;
		}


		if (b & CWIID_BTN_A) {
			access_action("Transport/ToggleRoll");
		}

		if (b & CWIID_BTN_1) { // 1
			access_action("Editor/track-record-enable-toggle");
		}
		if (b & CWIID_BTN_2) { // 2
			rec_enable_toggle();
		}

		// d-pad
		if (b & CWIID_BTN_LEFT) { // left
			access_action("Editor/nudge-playhead-backward");
		}
		if (b & CWIID_BTN_RIGHT) { // right
			access_action("Editor/nudge-playhead-forward");
		}
		if (b & CWIID_BTN_DOWN) { // down
			access_action("Editor/select-next-route");
		}
		if (b & CWIID_BTN_UP) { // up
			access_action("Editor/select-prev-route");
		}


		if (b & CWIID_BTN_PLUS) { // +
			access_action("Editor/temporal-zoom-in");
		}
		if (b & CWIID_BTN_MINUS) { // -
			access_action("Editor/temporal-zoom-out");
		}
		if (b & CWIID_BTN_HOME) { // "home"
			// no op, yet. any suggestions?
			access_action("Editor/playhead-to-edit");
		}

	}
}

void
WiimoteControlProtocol::update_led_state()
{
	ENSURE_WIIMOTE_THREAD(sigc::mem_fun(*this, &WiimoteControlProtocol::update_led_state));

	uint8_t state = 0;

	if (session->transport_rolling()) {
		state |= CWIID_LED1_ON;
	}

	if (session->actively_recording()) {
		state |= CWIID_LED4_ON;
	}

	cwiid_set_led(wiimote_handle, state);
}

void
WiimoteControlProtocol::wiimote_main()
{
	bdaddr_t bdaddr;
	unsigned char rpt_mode = 0;
	register_thread("Wiimote Discovery and Callback Thread");

wiimote_discovery:

	std::cerr << "Wiimote: discovering, press 1+2" << std::endl;

 	while (!wiimote_handle && !main_thread_quit) {
		bdaddr = *BDADDR_ANY;
		callback_thread_registered_for_ardour = false;
		wiimote_handle = cwiid_open(&bdaddr, 0);

		if (!wiimote_handle && !main_thread_quit) {
			sleep(1); 
			// We don't know whether the issue was a timeout or a configuration 
			// issue
		}
	}

	if (main_thread_quit) {
		// The corner case where the wiimote is bound at the same time as
		// the control protocol is destroyed
		if (wiimote_handle) {
			cwiid_close(wiimote_handle);
		}
		wiimote_handle = 0;

		std::cerr << "Wiimote Control Protocol stopped before connected to a wiimote" << std::endl;
		return;
	}

	std::cerr << "Wiimote: connected" << std::endl;
	WiimoteControlProtocol::button_state = 0;

	if (cwiid_enable(wiimote_handle, CWIID_FLAG_REPEAT_BTN)) {
		std::cerr << "cwiid_enable(), error" << std::endl;
		cwiid_close(wiimote_handle);
		wiimote_handle = 0;
		return;
	}
	if (cwiid_set_mesg_callback(wiimote_handle, wiimote_control_protocol_cwiid_callback)) {
		std::cerr << "cwiid_set_mesg_callback(), couldn't connect callback" << std::endl;
		cwiid_close(wiimote_handle);
		wiimote_handle = 0;
		return;
	} 
	if (cwiid_command(wiimote_handle, CWIID_CMD_RPT_MODE, CWIID_RPT_BTN)) {
		std::cerr << "cwiid_command(), RPT_MODE error" << std::endl;
		cwiid_close(wiimote_handle);
		wiimote_handle = 0;
		return;
	}

	rpt_mode |= CWIID_RPT_BTN;
	cwiid_enable(wiimote_handle, CWIID_FLAG_MESG_IFC);
	cwiid_set_rpt_mode(wiimote_handle, rpt_mode);

	transport_state_conn = session->TransportStateChange.connect(sigc::mem_fun(*this, &WiimoteControlProtocol::update_led_state));
	record_state_conn = session->RecordStateChanged.connect(sigc::mem_fun(*this, &WiimoteControlProtocol::update_led_state));

	std::cerr << "Wiimote: initialization done, waiting for callbacks / quit" << std::endl;

	while (!main_thread_quit) {
		slot_mutex.lock();
		while (slot_list.empty() && !main_thread_quit && !restart_discovery)
			slot_cond.wait(slot_mutex);

		if (main_thread_quit) {
			slot_mutex.unlock();
			break;
		}

		if (restart_discovery) {
			std::cerr << "Wiimote: closing wiimote and restarting discovery" << std::endl;
			if (wiimote_handle) {
				cwiid_close(wiimote_handle);
				wiimote_handle = 0;
			}
			slot_mutex.unlock();
			restart_discovery = false;
			goto wiimote_discovery;
		}

		sigc::slot<void> call_me = *slot_list.begin();
		slot_list.pop_front();
		slot_mutex.unlock();

		call_me();
	}


	std::cerr << "Wiimote: main thread stopped" << std::endl;
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
