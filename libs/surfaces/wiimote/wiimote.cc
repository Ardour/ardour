/*
    Copyright (C) 2009-2013 Paul Davis
    Authors: Sampo Savolainen, Jannis Pohlmann

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <iostream>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "ardour/debug.h"
#include "ardour/session.h"
#include "i18n.h"

#include "wiimote.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

#include "pbd/abstract_ui.cc" // instantiate template

void wiimote_control_protocol_mesg_callback (cwiid_wiimote_t *wiimote, int mesg_count, union cwiid_mesg mesg[], timespec *t);

WiimoteControlProtocol::WiimoteControlProtocol (Session& session)
	: ControlProtocol (session, X_("Wiimote"))
	, AbstractUI<WiimoteControlUIRequest> ("wiimote")
	, wiimote (0)
	, idle_source (0)
	, button_state (0)
	, callback_thread_registered (false)
{
}

WiimoteControlProtocol::~WiimoteControlProtocol ()
{
	stop ();
}

bool
WiimoteControlProtocol::probe ()
{
	return true;
}

int
WiimoteControlProtocol::set_active (bool yn)
{
	int result;

	DEBUG_TRACE (DEBUG::WiimoteControl, string_compose ("WiimoteControlProtocol::set_active init with yn: '%1'\n", yn));

	/* do nothing if the active state is not changing */
	if (yn == _active) {
		return 0;
	}

	if (yn) {
		/* activate Wiimote control surface */
		result = start ();
	} else {
		/* deactivate Wiimote control surface */
		result = stop ();
	}

	/* remember new active state */
	_active = yn;

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::set_active done\n");

	return result;
}

XMLNode&
WiimoteControlProtocol::get_state ()
{
	XMLNode *node = new XMLNode ("Protocol");
	node->add_property (X_("name"), ARDOUR::ControlProtocol::_name);
	node->add_property (X_("feedback"), "0");
	return *node;
}

int
WiimoteControlProtocol::set_state (const XMLNode&, int)
{
	return 0;
}

void
WiimoteControlProtocol::do_request (WiimoteControlUIRequest* req)
{
	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::do_request init\n");

	if (req->type == CallSlot) {
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		stop ();
	}

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::do_request done\n");
}

int
WiimoteControlProtocol::start ()
{
	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::start init\n");

	// update LEDs whenever the transport or recording state changes
	session->TransportStateChange.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&WiimoteControlProtocol::update_led_state, this), this);
	session->RecordStateChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&WiimoteControlProtocol::update_led_state, this), this);

	// start the Wiimote control UI; it will run in its own thread context
	BaseUI::run ();

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::start done\n");

	return 0;
}

int
WiimoteControlProtocol::stop ()
{
	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::stop init\n");

	// stop wiimote discovery, just in case
	stop_wiimote_discovery ();

	// close and reset the wiimote handle
	if (wiimote) {
		cwiid_close (wiimote);
		wiimote = 0;
		callback_thread_registered = false;
	}

	// stop the Wiimote control UI
	BaseUI::quit ();

	// no longer update the LEDs
	session_connections.drop_connections ();

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::stop done\n");

	return 0;
}

void
WiimoteControlProtocol::thread_init ()
{
	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::thread_init init\n");

	pthread_set_name (X_("wiimote"));

	// allow to make requests to the GUI and RT thread(s)
	PBD::notify_gui_about_thread_creation (X_("gui"), pthread_self (), X_("wiimote"), 2048);
	BasicUI::register_thread ("wiimote");

	// connect a Wiimote
	start_wiimote_discovery ();

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::thread_init done\n");
}

void
WiimoteControlProtocol::start_wiimote_discovery ()
{
	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::start_wiimote_discovery init\n");

	// connect to the Wiimote using an idle source
	Glib::RefPtr<Glib::IdleSource> source = Glib::IdleSource::create ();
	source->connect (sigc::mem_fun (*this, &WiimoteControlProtocol::connect_idle));
	source->attach (_main_loop->get_context ());

	// grab a reference on the underlying idle source to keep it around
	idle_source = source->gobj ();
	g_source_ref (idle_source);

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::start_wiimote_discovery done\n");
}

void
WiimoteControlProtocol::stop_wiimote_discovery ()
{
	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::stop_wiimote_discovery init\n");

	if (idle_source) {
		g_source_unref (idle_source);
		idle_source = 0;
	}

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::stop_wiimote_discovery done\n");
}

bool
WiimoteControlProtocol::connect_idle ()
{
	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::connect_idle init\n");

	bool retry = true;

	if (connect_wiimote ()) {
		stop_wiimote_discovery ();
		retry = false;
	}

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::connect_idle done\n");

	return retry;
}

bool
WiimoteControlProtocol::connect_wiimote ()
{
	// abort the discovery and do nothing else if we already have a Wiimote
	if (wiimote) {
		return true;
	}

	bool success = true;

	// if we don't have a Wiimote yet, try to discover it; if that
	// fails, wait for a short period of time and try again
	if (!wiimote) {
		cerr << "Wiimote: Not discovered yet, press 1+2 to connect" << endl;

		bdaddr_t bdaddr = {{ 0, 0, 0, 0, 0, 0 }};
		wiimote = cwiid_open (&bdaddr, 0);
		callback_thread_registered = false;
		if (!wiimote) {
			success = false;
		} else {
			// a Wiimote was discovered
			cerr << "Wiimote: Connected successfully" << endl;

			// attach the WiimoteControlProtocol object to the Wiimote handle
			if (cwiid_set_data (wiimote, this)) {
				cerr << "Wiimote: Failed to attach control protocol" << endl;
				success = false;
			}

			// clear the last button state to start processing events cleanly
			button_state = 0;
		}
	}

	// enable message based communication with the Wiimote
	if (success && cwiid_enable (wiimote, CWIID_FLAG_MESG_IFC)) {
		cerr << "Wiimote: Failed to enable message based communication" << endl;
		success = false;
	}

	// enable button events to be received from the Wiimote
	if (success && cwiid_command (wiimote, CWIID_CMD_RPT_MODE, CWIID_RPT_BTN)) {
		cerr << "Wiimote: Failed to enable button events" << endl;
		success = false;
	}

	// receive an event for every single button pressed, not just when
	// a different button was pressed than before
	if (success && cwiid_enable (wiimote, CWIID_FLAG_REPEAT_BTN)) {
		cerr << "Wiimote: Failed to enable repeated button events" << endl;
		success = false;
	}

	// be notified of new input events
	if (success && cwiid_set_mesg_callback (wiimote, wiimote_control_protocol_mesg_callback)) {
	}

	// reset Wiimote handle if the configuration failed
	if (!success && wiimote) {
		cwiid_close (wiimote);
		wiimote = 0;
		callback_thread_registered = false;
	}

	return success;
}

void
WiimoteControlProtocol::update_led_state ()
{
	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::update_led_state init\n");

	uint8_t state = 0;

	// do nothing if we do not have a Wiimote
	if (!wiimote) {
		DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::update_led_state no wiimote connected\n");
		return;
	}

	// enable LED1 if Ardour is playing
	if (session->transport_rolling ()) {
		DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::update_led_state playing, activate LED1\n");
		state |= CWIID_LED1_ON;
	}

	// enable LED4 if Ardour is recording
	if (session->actively_recording ()) {
		DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::update_led_state recording, activate LED4\n");
		state |= CWIID_LED4_ON;
	}

	// apply the LED state
	cwiid_set_led (wiimote, state);

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::update_led_state done\n");
}

void
WiimoteControlProtocol::wiimote_callback (int mesg_count, union cwiid_mesg mesg[])
{
	// register the cwiid callback thread if that hasn't happened yet
	if (!callback_thread_registered) {
		BasicUI::register_thread ("wiimote callback");
		callback_thread_registered = true;
	}

	for (int i = 0; i < mesg_count; i++) {
		// restart Wiimote discovery when receiving errors
		if (mesg[i].type == CWIID_MESG_ERROR) {
			cerr << "Wiimote: disconnected" << endl;
			cwiid_close (wiimote);
			wiimote = 0;
			callback_thread_registered = false;
			start_wiimote_discovery ();
			return;
		}

		// skip non-button events
		if (mesg[i].type != CWIID_MESG_BTN) {
			continue;
		}

		// drop buttons from the event that were already pressed before
		uint16_t b = mesg[i].btn_mesg.buttons & ~button_state;

		// remember new button state
		button_state = mesg[i].btn_mesg.buttons;

		if (button_state & CWIID_BTN_B) {
			// B + A = abort recording and jump back
			if (b & CWIID_BTN_A) {
				access_action ("Transport/ToggleRollForgetCapture");
			}

			// B + left = move playhead to previous region boundary
			if (b & CWIID_BTN_LEFT) {
				access_action ("Editor/playhead-to-previous-region-boundary");
			}

			// B + right = move playhead to next region boundary
			if (b & CWIID_BTN_RIGHT) {
				access_action ("Editor/playhead-to-next-region-boundary");
			}

			// B + up = move playhead to next marker
			if (b & CWIID_BTN_UP) {
				next_marker ();
			}

			// B + down = move playhead to prev marker
			if (b & CWIID_BTN_DOWN) {
				prev_marker ();
			}

			// B + Home = add marker at playhead
			if (b & CWIID_BTN_HOME) {
				access_action ("Editor/add-location-from-playhead");
			}

			// B + minus = move playhead to the start
			if (b & CWIID_BTN_MINUS) {
				access_action ("Transport/GotoStart");
			}

			// B + plus = move playhead to the end
			if (b & CWIID_BTN_PLUS) {
				access_action ("Transport/GotoEnd");
			}
		} else {
			// A = toggle playback
			if (b & CWIID_BTN_A) {
				access_action ("Transport/ToggleRoll");
			}

			// 1 = toggle recording on the current track
			if (b & CWIID_BTN_1) {
				access_action ("Editor/track-record-enable-toggle");
			}

			// 2 = enable recording in general
			if (b & CWIID_BTN_2) {
				rec_enable_toggle ();
			}

			// left = move playhead back a bit
			if (b & CWIID_BTN_LEFT) {
				access_action ("Editor/nudge-playhead-backward");
			}

			// right = move playhead forward a bit
			if (b & CWIID_BTN_RIGHT) {
				access_action ("Editor/nudge-playhead-forward");
			}

			// up = select previous track
			if (b & CWIID_BTN_UP) {
				access_action ("Editor/select-prev-route");
			}

			// down = select next track
			if (b & CWIID_BTN_DOWN) {
				access_action ("Editor/select-next-route");
			}

			// + = zoom in
			if (b & CWIID_BTN_PLUS) {
				access_action ("Editor/temporal-zoom-in");
			}

			// - = zoom out
			if (b & CWIID_BTN_MINUS) {
				access_action ("Editor/temporal-zoom-out");
			}

			// home = no-op
			if (b & CWIID_BTN_HOME) {
				access_action ("Editor/playhead-to-edit");
			}
		}
	}
}

void
wiimote_control_protocol_mesg_callback (cwiid_wiimote_t *wiimote, int mesg_count, union cwiid_mesg mesg[], timespec *)
{
	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::mesg_callback init\n");

	WiimoteControlProtocol *protocol = (WiimoteControlProtocol *)cwiid_get_data (wiimote);

	if (protocol) {
		protocol->wiimote_callback (mesg_count, mesg);
	}

	DEBUG_TRACE (DEBUG::WiimoteControl, "WiimoteControlProtocol::mesg_callback done\n");
}
