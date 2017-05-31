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

#include <libusb.h>

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

static const uint16_t vendorID = 0x0b33;
static const uint16_t productID = 0x0030;

static void LIBUSB_CALL event_callback (struct libusb_transfer* transfer);


ShuttleproControlProtocol::ShuttleproControlProtocol (Session& session)
	: ControlProtocol (session, X_("Shuttlepro"))
	,  AbstractUI<ShuttleproControlUIRequest> ("shuttlepro")
	, _io_source (0)
	, _dev_handle (0)
	, _usb_transfer (0)
	, _supposed_to_quit (false)
{
	libusb_init (0);
	//libusb_set_debug(0, LIBUSB_LOG_LEVEL_WARNING);

	BaseUI::run();
}

ShuttleproControlProtocol::~ShuttleproControlProtocol ()
{
	stop ();
	libusb_exit (0);
	BaseUI::quit();
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

void
ShuttleproControlProtocol::thread_init () {
	DEBUG_TRACE (DEBUG::ShuttleproControl, "thread_init()\n");

	pthread_set_name (X_("shuttlepro"));
	PBD::notify_event_loops_about_thread_creation (pthread_self (), X_("shuttlepro"), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (X_("shuttlepro"), 128);

	DEBUG_TRACE (DEBUG::ShuttleproControl, "thread_init() fin\n")
}

bool
ShuttleproControlProtocol::wait_for_event ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "wait_for_event\n");
	if (!_supposed_to_quit) {
		libusb_handle_events (0);
	}

	return true;
}


int
ShuttleproControlProtocol::aquire_device ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "aquire_device()\n");

	int err;

	if (_dev_handle) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "already have a device handle\n");
		return -1;
	}

	if ((_dev_handle = libusb_open_device_with_vid_pid (NULL, vendorID, productID)) == 0) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "failed to open USB handle\n");
		return -1;
	}

	if (libusb_kernel_driver_active (_dev_handle, 0)) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "Detatching kernel driver\n");
		err = libusb_detach_kernel_driver (_dev_handle, 0);
		if (err < 0) {
			DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose("could not detatch kernel driver %d\n", err));
			goto usb_close;
		}
	}

	if ((err = libusb_claim_interface (_dev_handle, 0x00))) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "failed to claim USB device\n");
		goto usb_close;
	}

	_usb_transfer = libusb_alloc_transfer (0);
	if (!_usb_transfer) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "failed to alloc usb transfer\n");
		err = -ENOMEM;
		goto usb_close;
	}

	libusb_fill_interrupt_transfer(_usb_transfer, _dev_handle, 1 | LIBUSB_ENDPOINT_IN, _buf, sizeof(_buf),
				       event_callback, this, 0);

	DEBUG_TRACE (DEBUG::ShuttleproControl, "callback installed\n");

	if ((err = libusb_submit_transfer (_usb_transfer))) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose("failed to submit tansfer: %1\n", err));
		goto free_transfer;
	}

	return 0;

 free_transfer:
	libusb_free_transfer (_usb_transfer);

 usb_close:
	libusb_close (_dev_handle);
	_dev_handle = 0;
	return err;
}

void
ShuttleproControlProtocol::release_device()
{
	if (!_dev_handle) {
		return;
	}

	libusb_close(_dev_handle);
	libusb_free_transfer(_usb_transfer);
	libusb_release_interface(_dev_handle, 0);
	_usb_transfer = 0;
	_dev_handle = 0;
}

int
ShuttleproControlProtocol::start ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "start()\n");

	_supposed_to_quit = false;

	int err = aquire_device();
	if (err) {
		return err;
	}

	if (!_dev_handle) {
		return -1;
	}

	_state.shuttle = 0;
	_state.jog = 0;
	_state.buttons = 0;

	Glib::RefPtr<Glib::IdleSource> source = Glib::IdleSource::create ();
	source->connect (sigc::mem_fun (*this, &ShuttleproControlProtocol::wait_for_event));
	source->attach (_main_loop->get_context ());

	_io_source = source->gobj ();
	g_source_ref (_io_source);

	DEBUG_TRACE (DEBUG::ShuttleproControl, "start() fin\n");
	return 0;
}


int
ShuttleproControlProtocol::stop ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "stop()\n");

	_supposed_to_quit = true;

	if (_io_source) {
		g_source_destroy (_io_source);
		g_source_unref (_io_source);
		_io_source = 0;
	}

	if (_dev_handle) {
		release_device ();
	}

	DEBUG_TRACE (DEBUG::ShuttleproControl, "stop() fin\n");
	return 0;
}

void
ShuttleproControlProtocol::handle_event () {
	if (_usb_transfer->status == LIBUSB_TRANSFER_TIMED_OUT) {
		goto resubmit;
	}
	if (_usb_transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose("libusb_transfer not completed: %1\n", _usb_transfer->status));
		stop ();
		return;
	}

	State new_state;
	new_state.shuttle = _buf[0];
	new_state.jog = _buf[1];
	new_state.buttons = (_buf[4] << 8) + _buf[3];

//	cout << "event " << (int)new_state.shuttle << " " << (int)new_state.jog << " " << (int)new_state.buttons << endl;;

	for (uint8_t btn=0; btn<16; btn++) {
                if ( (new_state.buttons & (1<<btn)) && !(_state.buttons & (1<<btn)) ) {
                        handle_button_press (btn);
                } else if ( !(new_state.buttons & (1<<btn)) && (_state.buttons & (1<<btn)) ) {
                        // we might handle button releases one day
                }
        }

	if (new_state.jog == 255 && _state.jog == 0) {
		jog_event_backward ();
	} else if (new_state.jog == 0 && _state.jog == 255) {
		jog_event_forward();
	} else if (new_state.jog < _state.jog) {
		jog_event_backward();
	} else if (new_state.jog > _state.jog) {
		jog_event_forward();
	}

	if (new_state.shuttle != _state.shuttle) {
		shuttle_event(new_state.shuttle);
	}

	_state = new_state;

 resubmit:
	if (libusb_submit_transfer (_usb_transfer)) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, "failed to resubmit usb transfer after callback\n");
		stop ();
	}
}

/* The buttons have the following layout
 *
 *          01  02  03  04
 *        05  06  07  08  09
 *
 *          15   Jog   15
 *
 *            10     11
 *            12     13
 */

enum Buttons {
	BTN01 =  0, BTN02 =  1, BTN03 =  2, BTN04 =  3, BTN05 =  4, BTN06 =  5, BTN07 =  6,
	BTN08 =  7, BTN09 =  8, BTN10 =  9, BTN11 = 10, BTN12 = 11, BTN13 = 12, BTN14 = 13, BTN15 = 14
};

void
ShuttleproControlProtocol::handle_button_press (unsigned short btn)
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("Shuttlepro button number %1\n", btn+1));
	switch (btn) {
	case BTN01: midi_panic (); break;
	case BTN02: access_action ("Editor/remove-last-capture"); break;

// FIXME: calling undo () and redo () from here makes ardour crash (see #7371)
//	case BTN03: undo (); break;
//	case BTN04: redo (); break;

	case BTN06: set_record_enable (!get_record_enabled ()); break;
	case BTN07: transport_stop (); break;
	case BTN08: transport_play (); break;

	case BTN05: prev_marker (); break;
	case BTN09: next_marker (); break;

	case BTN14: goto_start (); break;
	case BTN15: goto_end (); break;

	case BTN10: jump_by_bars (-4.0); break;
	case BTN11: jump_by_bars (+4.0); break;

	case BTN13: add_marker (); break;

	default: break;
	}
}

void
ShuttleproControlProtocol::jog_event_backward ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "jog event backward\n");
	jump_by_beats (-1.0);
}

void
ShuttleproControlProtocol::jog_event_forward ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "jog event forward\n");
	jump_by_beats (+1.0);
}

void
ShuttleproControlProtocol::shuttle_event(int position)
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("shuttle event %1\n", position));
	set_transport_speed(double(position));
}

static void LIBUSB_CALL event_callback (libusb_transfer* transfer)
{
	ShuttleproControlProtocol* spc = static_cast<ShuttleproControlProtocol*> (transfer->user_data);
	spc->handle_event();
}
