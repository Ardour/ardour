/*
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ardour_wiimote_control_protocol_h
#define ardour_wiimote_control_protocol_h

#include <cwiid.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

struct WiimoteControlUIRequest : public BaseUI::BaseRequestObject {
public:
	WiimoteControlUIRequest () {}
	~WiimoteControlUIRequest () {}
};

class WiimoteControlProtocol
	: public ARDOUR::ControlProtocol
	, public AbstractUI<WiimoteControlUIRequest>
{
public:
	WiimoteControlProtocol (ARDOUR::Session &);
	virtual ~WiimoteControlProtocol ();

	static bool probe ();
	static void* request_factory (uint32_t);

	int set_active (bool yn);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	void start_wiimote_discovery ();
	void stop_wiimote_discovery ();

	void wiimote_callback (int mesg_count, union cwiid_mesg mesg[]);

	void stripable_selection_changed () {}

protected:
	void do_request (WiimoteControlUIRequest*);
	int start ();
	int stop ();

	void thread_init ();

	bool connect_idle ();
	bool connect_wiimote ();

	void update_led_state ();

protected:
	PBD::ScopedConnectionList session_connections;
	cwiid_wiimote_t* wiimote;
	GSource *idle_source;
	uint16_t button_state;
	bool callback_thread_registered;
};

#endif  /* ardour_wiimote_control_protocol_h */

