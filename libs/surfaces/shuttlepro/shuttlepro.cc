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
	, _shuttle_was_zero (true)
	, _was_rolling_before_shuttle (false)
	, _keep_rolling (true)
	, _shuttle_speeds ( { 0.50, 0.75, 1.0, 1.5, 2.0, 5.0, 10.0 } )
	, _jog_distance ( { .value = 1.0, .unit = BEATS } )
	, _gui (0)
{
	libusb_init (0);
	//libusb_set_debug(0, LIBUSB_LOG_LEVEL_WARNING);

	setup_default_button_actions ();
	BaseUI::run();
}

ShuttleproControlProtocol::~ShuttleproControlProtocol ()
{
	stop ();
	libusb_exit (0);
	BaseUI::quit();
	tear_down_gui ();
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
	node.set_property (X_("keep-rolling"), _keep_rolling);

	ostringstream os;
	for (vector<double>::const_iterator it = _shuttle_speeds.begin (); it != _shuttle_speeds.end (); ++it) {
		os << *it << ' ';
	}
	string s = os.str ();
	s.pop_back ();
	node.set_property (X_("shuttle-speeds"), s);

	node.set_property (X_("jog-distance"), _jog_distance.value);
	switch (_jog_distance.unit) {
	case SECONDS: s = X_("seconds"); break;
	case BARS: s = X_("bars"); break;
	case BEATS:
	default: s = X_("beats");
	}
	node.set_property (X_("jog-unit"), s);

	for (unsigned int i=0; i<_button_actions.size(); ++i) {
		XMLNode* child = new XMLNode (string_compose (X_("button-%1"), i+1));
		node.add_child_nocopy (_button_actions[i]->get_state (*child));
	}

	return node;
}

int
ShuttleproControlProtocol::set_state (const XMLNode& node, int version)
{
	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}
	node.get_property (X_("keep-rolling"), _keep_rolling);

	string s;
	node.get_property (X_("shuttle-speeds"), s);
	istringstream is (s);
	for (vector<double>::iterator it = _shuttle_speeds.begin (); it != _shuttle_speeds.end (); ++it) {
		is >> *it;
	}

	node.get_property (X_("jog-distance"), _jog_distance.value);
	node.get_property (X_("jog-unit"), s);
	if (s == "seconds") {
		_jog_distance.unit = SECONDS;
	} else if (s == "bars") {
		_jog_distance.unit = BARS;
	} else {
		_jog_distance.unit = BEATS;
	}

	XMLNode* child;
	for (unsigned int i=0; i<_button_actions.size(); ++i) {
		if ((child = node.child (string_compose(X_("button-%1"), i+1).c_str())) == 0) {
			cout << "Button " << i+1 << " not found" << endl;
			continue;
		}
		string type;
		child->get_property (X_("type"), type);
		if (type == X_("action")) {
			string path ("");
			child->get_property (X_("path"), path);
			cout << "Button " << i+1 << " " << path << endl;
			boost::shared_ptr<ButtonBase> b (new ButtonAction (path, *this));
			_button_actions[i] = b;
		} else {
			double value;
			child->get_property(X_("value"), value);

			string s;
			child->get_property(X_("unit"), s);
			JumpUnit unit;
			if (s == X_("seconds")) {
				unit = SECONDS;
			} else if (s == X_("bars")) {
				unit = BARS;
			} else {
				unit = BEATS;
			}

			boost::shared_ptr<ButtonBase> b (new ButtonJump (JumpDistance (value, unit), *this));
		}
	}

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
ShuttleproControlProtocol::thread_init ()
{
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
			DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("could not detatch kernel driver %d\n", err));
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

	libusb_fill_interrupt_transfer (_usb_transfer, _dev_handle, 1 | LIBUSB_ENDPOINT_IN, _buf, sizeof(_buf),
				       event_callback, this, 0);

	DEBUG_TRACE (DEBUG::ShuttleproControl, "callback installed\n");

	if ((err = libusb_submit_transfer (_usb_transfer))) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("failed to submit tansfer: %1\n", err));
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
ShuttleproControlProtocol::release_device ()
{
	if (!_dev_handle) {
		return;
	}

	libusb_close (_dev_handle);
	libusb_free_transfer (_usb_transfer);
	libusb_release_interface (_dev_handle, 0);
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


boost::shared_ptr<ButtonBase>
ShuttleproControlProtocol::make_button_action (string action_string)
{
	return boost::shared_ptr<ButtonBase> (new ButtonAction (action_string, *this));
}

/* The buttons have the following layout
 *
 *          00  01  02  03
 *        04  05  06  07  08
 *
 *          13   Jog   14
 *
 *            09     10
 *            11     12
 */

void
ShuttleproControlProtocol::setup_default_button_actions ()
{
 	_button_actions.push_back (make_button_action ("MIDI/panic"));
	_button_actions.push_back (make_button_action ("Editor/remove-last-capture"));
	_button_actions.push_back (make_button_action ("Editor/undo"));
	_button_actions.push_back (make_button_action ("Editor/redo"));
	_button_actions.push_back (make_button_action ("Common/jump-backward-to-mark"));
	_button_actions.push_back (make_button_action ("Transport/Record"));
	_button_actions.push_back (make_button_action ("Transport/Stop"));
	_button_actions.push_back (make_button_action ("Transport/Roll"));
	_button_actions.push_back (make_button_action ("Common/jump-forward-to-mark"));
	_button_actions.push_back (boost::shared_ptr<ButtonBase> (new ButtonJump (JumpDistance (-4.0, BARS), *this)));
	_button_actions.push_back (boost::shared_ptr<ButtonBase> (new ButtonJump (JumpDistance (+4.0, BARS), *this)));
	_button_actions.push_back (make_button_action (""));
	_button_actions.push_back (make_button_action ("Common/add-location-from-playhead"));
	_button_actions.push_back (make_button_action ("Transport/GotoStart"));
	_button_actions.push_back (make_button_action ("Transport/GotoEnd"));
}

void
ShuttleproControlProtocol::handle_button_press (unsigned short btn)
{
	if (btn >= _button_actions.size ()) {
		DEBUG_TRACE (DEBUG::ShuttleproControl,
			     string_compose ("Shuttlepro button number out of bounds %1, max is %2\n",
					     btn, _button_actions.size ()));
		return;
	}

	_button_actions[btn]->execute ();
}

void
ShuttleproControlProtocol::prev_marker_keep_rolling ()
{
	framepos_t pos = session->locations()->first_mark_before (session->transport_frame());

	if (pos >= 0) {
		session->request_locate (pos, _keep_rolling && session->transport_rolling());
	} else {
		session->goto_start ();
	}
}

void
ShuttleproControlProtocol::next_marker_keep_rolling ()
{
	framepos_t pos = session->locations()->first_mark_after (session->transport_frame());

	if (pos >= 0) {
		session->request_locate (pos, _keep_rolling && session->transport_rolling());
	} else {
		session->goto_end();
	}
}

void
ShuttleproControlProtocol::jog_event_backward ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "jog event backward\n");
	jump_backward (_jog_distance);
}

void
ShuttleproControlProtocol::jog_event_forward ()
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, "jog event forward\n");
	jump_forward (_jog_distance);
}

void
ShuttleproControlProtocol::jump_forward (JumpDistance dist)
{
	bool kr = _keep_rolling && session->transport_rolling ();
	switch (dist.unit) {
	case SECONDS: jump_by_seconds (dist.value, kr); break;
	case BEATS: jump_by_beats (dist.value, kr); break;
	case BARS: jump_by_bars (dist.value, kr); break;
	default: break;
	}
}

void ShuttleproControlProtocol::jump_backward (JumpDistance dist)
{
	JumpDistance bw = dist;
	bw.value = -bw.value;
	jump_forward(bw);
}

void
ShuttleproControlProtocol::shuttle_event(int position)
{
	DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("shuttle event %1\n", position));
	if (position != 0) {
		if (_shuttle_was_zero) {
			_was_rolling_before_shuttle = session->transport_rolling ();
		}
		double speed = position > 0 ? _shuttle_speeds[position-1] : -_shuttle_speeds[-position-1];
		set_transport_speed (speed);
		_shuttle_was_zero = false;
	} else {
		if (_keep_rolling && _was_rolling_before_shuttle) {
			set_transport_speed (1.0);
		} else {
			transport_stop ();
		}
		_shuttle_was_zero = true;
	}
}

void
ButtonJump::execute ()
{
	_spc.jump_forward (_dist);
}

XMLNode&
ButtonJump::get_state (XMLNode& node) const
{
	string ts (X_("jump"));
	node.set_property (X_("type"), ts);
	node.set_property (X_("distance"), _dist.value);

	string s;
	switch (_dist.unit) {
	case SECONDS: s = X_("seconds"); break;
	case BARS: s = X_("bars"); break;
	case BEATS:
	default: s = X_("beats");
	}
	node.set_property (X_("unit"), s);

	return node;
}

void
ButtonAction::execute ()
{
	_spc.access_action (_action_string);
}


XMLNode&
ButtonAction::get_state (XMLNode& node) const
{
	string ts (X_("action"));
	node.set_property (X_("type"), ts);
	node.set_property (X_("path"), _action_string);

	return node;
}

static void LIBUSB_CALL event_callback (libusb_transfer* transfer)
{
	ShuttleproControlProtocol* spc = static_cast<ShuttleproControlProtocol*> (transfer->user_data);
	spc->handle_event();
}
