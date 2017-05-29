/*
    Copyright (C) 2009-2013 Paul Davis
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

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#include <glibmm.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "ardour/debug.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "pbd/i18n.h"

#include "shuttlepro.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace std;
using namespace ArdourSurface;

#include "pbd/abstract_ui.cc" // instantiate template


ShuttleproControlProtocol::ShuttleproControlProtocol (Session& session)
	: ControlProtocol (session, X_("Shuttlepro"))
	,  AbstractUI<ShuttleproControlUIRequest> ("shuttlepro")
	, _io_source (0)
	, _file_descriptor (-1)
	, _jog_position (-1)
{
}

ShuttleproControlProtocol::~ShuttleproControlProtocol ()
{
	stop ();
}

bool
ShuttleproControlProtocol::probe ()
{
	return true;
}

int
ShuttleproControlProtocol::set_active (bool yn)
{
	int result;

	DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("set_active() init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {
		result = start ();
	} else {
		result = stop ();
	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::ShuttleproControl, "set_active() fin\n");

	return result;
}

XMLNode&
ShuttleproControlProtocol::get_state ()
{
	XMLNode& node (ControlProtocol::get_state());
	node.set_property (X_("feedback"), "0");
	return node;
}

int
ShuttleproControlProtocol::set_state (const XMLNode&, int)
{
	return 0;
}

void
ShuttleproControlProtocol::do_request (ShuttleproControlUIRequest* req)
{
	if (req->type == CallSlot) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "do_request type CallSlot\n");
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "do_request type Quit\n");
		stop ();
	}
}

int discover_shuttlepro_fd ();

void
ShuttleproControlProtocol::thread_init () {
	DEBUG_TRACE (DEBUG::ShuttleproControl, "thread_init()\n");

	BasicUI::register_thread (X_("Shuttlepro"));

	_file_descriptor = discover_shuttlepro_fd ();
	if (_file_descriptor < 0) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "No ShuttlePRO device found\n");
		return;
	}

	_shuttle_position = 0;
	_old_shuttle_position = 0;
	_shuttle_event_recieved = false;

	Glib::RefPtr<Glib::IOSource> source = Glib::IOSource::create (_file_descriptor, IO_IN | IO_ERR);
	source->connect (sigc::bind (sigc::mem_fun (*this, &ShuttleproControlProtocol::input_event), _file_descriptor));
	source->attach (_main_loop->get_context ());

	_io_source = source->gobj ();
	g_source_ref (_io_source);

	DEBUG_TRACE (DEBUG::ShuttleproControl, "thread_init() fin\n")
}

int
ShuttleproControlProtocol::start ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "start()\n");

	BaseUI::run();

	DEBUG_TRACE (DEBUG::ShuttleproControl, "start() fin\n");
	return 0;
}


int
ShuttleproControlProtocol::stop ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "stop()\n");

	if (_io_source) {
		g_source_destroy (_io_source);
		g_source_unref (_io_source);
		_io_source = 0;

		close(_file_descriptor);
		_file_descriptor = -1;
	}

	BaseUI::quit();

	DEBUG_TRACE (DEBUG::ShuttleproControl, "stop() fin\n");
	return 0;
}

void
ShuttleproControlProtocol::handle_event (EV ev) {
//	cout << "event " << ev.type << " " << ev.code << " " << ev.value << endl;;

	if (ev.type == 0) { // check if shuttle is turned
		if (_shuttle_event_recieved) {
			if (_shuttle_position != _old_shuttle_position) {
				shuttle_event (_shuttle_position);
				_old_shuttle_position = _shuttle_position;
			}
			_shuttle_event_recieved = false;
		} else {
			if (_shuttle_position != 0) {
				shuttle_event (0);
				_shuttle_position = 0;
				_old_shuttle_position = 0;
			}
		}
	}

	if (ev.type == 1) { // key
		if (ev.value == 1) {
			handle_key_press (ev.code-255);
		}
		return;
	}

	if (ev.code == 7) { // jog wheel
		if (_jog_position == ev.value) {
			return;
		}
		if (_jog_position == -1) { // first jog event needed to get orientation
			_jog_position = ev.value;
			return;
		}
		if (ev.value == 255 && _jog_position == 1) {
			jog_event_backward ();
		} else if (ev.value == 1 && _jog_position == 255) {
			jog_event_forward();
		} else if (ev.value < _jog_position) {
			jog_event_backward();
		} else {
			jog_event_forward();
		}
		_jog_position = ev.value;

		return;
	}

	if (ev.code == 8) { // shuttle wheel
		_shuttle_event_recieved = true;
		_shuttle_position = ev.value;
		return;
	}
}

/* The keys have the following layout
 *
 *          1   2   3   4
 *        5   6   7   8   9
 *
 *        14    Jog     15
 *
 *           10     11
 *           12     13
 */

enum Key {
	KEY01 = 1, KEY02 = 2, KEY03 = 3, KEY04 = 4, KEY05 = 5, KEY06 = 6, KEY07 = 7,
	KEY08 = 8, KEY09 = 9, KEY10 = 10, KEY11 = 11, KEY12 = 12, KEY13 = 13, KEY14 = 14, KEY15 = 15
};

void
ShuttleproControlProtocol::handle_key_press (unsigned short key)
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("Shuttlepro key number %1\n", key));
	switch (key) {
	case KEY01: midi_panic (); break;
	case KEY02: access_action ("Editor/remove-last-capture"); break;

// FIXME: calling undo () and redo () from here makes ardour crash (see #7371)
//	case KEY03: undo (); break;
//	case KEY04: redo (); break;

	case KEY06: set_record_enable (!get_record_enabled ()); break;
	case KEY07: transport_stop (); break;
	case KEY08: transport_play (); break;

	case KEY05: prev_marker (); break;
	case KEY09: next_marker (); break;

	case KEY14: goto_start (); break;
	case KEY15: goto_end (); break;

	case KEY10: jump_by_bars (-4.0); break;
	case KEY11: jump_by_bars (+4.0); break;

	case KEY13: add_marker (); break;

	default: break;
	}
}

void
ShuttleproControlProtocol::jog_event_backward ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "jog event backward\n");
	jump_by_beats (-0.5);
}

void
ShuttleproControlProtocol::jog_event_forward ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "jog event forward\n");
	jump_by_beats (+0.5);
}

void
ShuttleproControlProtocol::shuttle_event(int position)
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("shuttle event %1\n", position));
	set_transport_speed(double(position));
}

bool
ShuttleproControlProtocol::input_event (IOCondition ioc, int fd)
{
	if (ioc & ~IO_IN) {
		return false;
	}

	EV ev;
	size_t r;

	r = read (fd, &ev, sizeof (ev));
	if (r < sizeof (ev)) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "Received too small event, strange.\n");
		return false;
	}
	handle_event (ev);

	return true;
}

int
discover_shuttlepro_fd () {
        DIR* dir = opendir ("/dev/input");
        if (!dir) {
                DEBUG_TRACE (DEBUG::ShuttleproControl, "Could not open /dev/input\n");
                return -1;
        }

        struct dirent* item = 0;

        char dev_name[256];

        while ((item = readdir (dir))) {
                if (!item) {
                        continue;
                }

                if (strncmp (item->d_name, "event", 5)) {
                        continue;
                }

                int fd = open (Glib::build_filename ("/dev/input", item->d_name).c_str (), O_RDONLY);
                if (fd < 0) {
                        continue;
                }

                memset (dev_name, 0, sizeof (dev_name));
                ioctl (fd, EVIOCGNAME (sizeof (dev_name)), dev_name);

		if (!strcmp (dev_name, "Contour Design ShuttlePRO v2")) {
			DEBUG_TRACE (DEBUG::ShuttleproControl, "Shuttlepro found\n");
			return fd;
		}
        }
	return -1;
}
