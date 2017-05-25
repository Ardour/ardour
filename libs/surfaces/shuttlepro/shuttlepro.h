/*
    Copyright (C) 2009-2017 Paul Davis
    Author: Johannes Mueller

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

#ifndef ardour_shuttlepro_control_protocol_h
#define ardour_shuttlepro_control_protocol_h

#include <glibmm/main.h>

#include <linux/input.h>
typedef struct input_event EV;

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

struct ShuttleproControlUIRequest : public BaseUI::BaseRequestObject {
public:
	ShuttleproControlUIRequest () {}
	~ShuttleproControlUIRequest () {}
};


class ShuttleproControlProtocol
	: public ARDOUR::ControlProtocol
	, public AbstractUI<ShuttleproControlUIRequest>
{
public:
	ShuttleproControlProtocol (ARDOUR::Session &);
	virtual ~ShuttleproControlProtocol ();

	static bool probe ();
	static void* request_factory (uint32_t);

	int set_active (bool yn);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

//	void start_shuttlepro_discovery ();
//	void stop_shuttlepro_discovery ();

	void stripable_selection_changed () {}

private:
	void do_request (ShuttleproControlUIRequest*);
	int start ();
	int stop ();

	void thread_init ();

	int discover_shuttlepro_fd ();

	void handle_event (EV event);
	void handle_key_press (unsigned short key);

	void jog_event_backward();
	void jog_event_forward();

	void shuttle_event(int position);

	bool input_event (Glib::IOCondition ioc, int fd);
	GSource* _io_source;
	int _file_descriptor;

	int _jog_position;
	int _shuttle_position, _old_shuttle_position;
	bool _shuttle_event_recieved;
};

#endif  /* ardour_shuttlepro_control_protocol_h */
