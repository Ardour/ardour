/*
 * Copyright (C) 2019 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include <iostream>

#include <libusb.h>

#include <glibmm.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "ardour/debug.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "pbd/i18n.h"

#include "contourdesign.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace std;
using namespace ArdourSurface;

#include "pbd/abstract_ui.cc" // instantiate template

static const uint16_t ContourDesign = 0x0b33;
static const uint16_t ShuttlePRO_id = 0x0010;
static const uint16_t ShuttlePRO_v2_id = 0x0030;
static const uint16_t ShuttleXpress_id = 0x0020;

static void LIBUSB_CALL event_callback (struct libusb_transfer* transfer);


ContourDesignControlProtocol::ContourDesignControlProtocol (Session& session)
	: ControlProtocol (session, X_("ContourDesign"))
	,  AbstractUI<ContourDesignControlUIRequest> ("contourdesign")
	, _io_source (0)
	, _dev_handle (0)
	, _usb_transfer (0)
	, _supposed_to_quit (false)
	, _device_type (None)
	, _shuttle_was_zero (true)
	, _was_rolling_before_shuttle (false)
	, _test_mode (false)
	, _keep_rolling (true)
	, _shuttle_speeds ()
	, _jog_distance ()
	, _gui (0)
{
	libusb_init (0);
//	libusb_set_debug(0, LIBUSB_LOG_LEVEL_WARNING);

	_shuttle_speeds.push_back (0.50);
	_shuttle_speeds.push_back (0.75);
	_shuttle_speeds.push_back (1.0);
	_shuttle_speeds.push_back (1.5);
	_shuttle_speeds.push_back (2.0);
	_shuttle_speeds.push_back (5.0);
	_shuttle_speeds.push_back (10.0);

	setup_default_button_actions ();
	BaseUI::run();
}

ContourDesignControlProtocol::~ContourDesignControlProtocol ()
{
	stop ();
	libusb_exit (0);
	BaseUI::quit();
	tear_down_gui ();
}

bool
ContourDesignControlProtocol::probe ()
{
	bool rv = LIBUSB_SUCCESS == libusb_init (0);
	if (rv) {
		libusb_exit (0);
	}
	return rv;
}

void*
ContourDesignControlProtocol::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	 * instantiated in this source module. To provide something visible for
	 * use in the interface/descriptor, we have this static method that is
	 * template-free.
	 */
	return request_buffer_factory (num_requests);
}

int
ContourDesignControlProtocol::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::ContourDesignControl, string_compose ("set_active() init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {
		start ();
	} else {
		stop ();
	}

	ControlProtocol::set_active (yn);

	return _error;
}

XMLNode&
ContourDesignControlProtocol::get_state ()
{
	XMLNode& node (ControlProtocol::get_state());
	node.set_property (X_("keep-rolling"), _keep_rolling);

	ostringstream os;
	vector<double>::const_iterator it = _shuttle_speeds.begin ();
	os << *(it++);
	for (; it != _shuttle_speeds.end (); ++it) {
		os << ' ' << *it;
	}
	string s = os.str ();
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
ContourDesignControlProtocol::set_state (const XMLNode& node, int version)
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
			continue;
		}
		string type;
		child->get_property (X_("type"), type);
		if (type == X_("action")) {
			string path ("");
			child->get_property (X_("path"), path);
			boost::shared_ptr<ButtonBase> b (new ButtonAction (path, *this));
			_button_actions[i] = b;
		} else {
			double value;
			string s;

			if (!child->get_property(X_("value"), value)) {
				continue;
			}
			if (!child->get_property(X_("unit"), s)) {
				continue;
			}

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
ContourDesignControlProtocol::do_request (ContourDesignControlUIRequest* req)
{
	if (req->type == CallSlot) {
		DEBUG_TRACE (DEBUG::ContourDesignControl, "do_request type CallSlot\n");
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		DEBUG_TRACE (DEBUG::ContourDesignControl, "do_request type Quit\n");
		stop ();
	}
}

void
ContourDesignControlProtocol::thread_init ()
{
	DEBUG_TRACE (DEBUG::ContourDesignControl, "thread_init()\n");

	pthread_set_name (X_("contourdesign"));
	PBD::notify_event_loops_about_thread_creation (pthread_self (), X_("contourdesign"), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (X_("contourdesign"), 128);

	set_thread_priority ();
}

bool
ContourDesignControlProtocol::wait_for_event ()
{
	DEBUG_TRACE (DEBUG::ContourDesignControl, "wait_for_event\n");
	if (!_supposed_to_quit) {
		libusb_handle_events (0);
	}

	return true;
}

int
get_usb_device (uint16_t vendor_id, uint16_t product_id, libusb_device** device)
{
	struct libusb_device **devs;
	struct libusb_device *dev;
	size_t i = 0;
	int r = LIBUSB_ERROR_NO_DEVICE;

	*device = 0;

	if (libusb_get_device_list (0, &devs) < 0) {
		return LIBUSB_ERROR_NO_DEVICE;
	}

	while ((dev = devs[i++])) {
		struct libusb_device_descriptor desc;
		r = libusb_get_device_descriptor (dev, &desc);
		if (r != LIBUSB_SUCCESS) {
			goto out;
		}
		if (desc.idVendor == vendor_id && desc.idProduct == product_id) {
			*device = dev;
			break;
		}
	}

out:
	libusb_free_device_list(devs, 1);
	if (!dev && r == LIBUSB_SUCCESS) {
		return LIBUSB_ERROR_NO_DEVICE;
	}
	return r;
}

int
ContourDesignControlProtocol::acquire_device ()
{
	DEBUG_TRACE (DEBUG::ContourDesignControl, "acquire_device()\n");

	int err;

	if (_dev_handle) {
		DEBUG_TRACE (DEBUG::ContourDesignControl, "already have a device handle\n");
		return LIBUSB_SUCCESS;
	}

	libusb_device* dev;


	if ((err = get_usb_device (ContourDesign, ShuttleXpress_id, &dev)) == LIBUSB_SUCCESS) {
		_device_type = ShuttleXpress;
	}
	else if ((err = get_usb_device (ContourDesign, ShuttlePRO_id, &dev)) == LIBUSB_SUCCESS) {
		_device_type = ShuttlePRO;
	}
	else if ((err = get_usb_device (ContourDesign, ShuttlePRO_v2_id, &dev)) == LIBUSB_SUCCESS) {
		_device_type = ShuttlePRO_v2;
	}
	else {
		_device_type = None;
		return err;
	}

	err = libusb_open (dev, &_dev_handle);
	if (err != LIBUSB_SUCCESS) {
		return err;
	}

	libusb_set_auto_detach_kernel_driver (_dev_handle, true);

	if ((err = libusb_claim_interface (_dev_handle, 0x00)) != LIBUSB_SUCCESS) {
		DEBUG_TRACE (DEBUG::ContourDesignControl, "failed to claim USB device\n");
		goto usb_close;
	}

	_usb_transfer = libusb_alloc_transfer (0);
	if (!_usb_transfer) {
		DEBUG_TRACE (DEBUG::ContourDesignControl, "failed to alloc usb transfer\n");
		err = LIBUSB_ERROR_NO_MEM;
		goto usb_close;
	}

	libusb_fill_interrupt_transfer (_usb_transfer, _dev_handle, 1 | LIBUSB_ENDPOINT_IN, _buf, sizeof(_buf),
				       event_callback, this, 0);

	DEBUG_TRACE (DEBUG::ContourDesignControl, "callback installed\n");

	if ((err = libusb_submit_transfer (_usb_transfer)) != LIBUSB_SUCCESS) {
		DEBUG_TRACE (DEBUG::ContourDesignControl, string_compose ("failed to submit tansfer: %1\n", err));
		goto free_transfer;
	}

	return LIBUSB_SUCCESS;

 free_transfer:
	libusb_free_transfer (_usb_transfer);

 usb_close:
	libusb_close (_dev_handle);
	_dev_handle = 0;
	return err;
}

void
ContourDesignControlProtocol::release_device ()
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

void
ContourDesignControlProtocol::start ()
{
	DEBUG_TRACE (DEBUG::ContourDesignControl, "start()\n");

	_supposed_to_quit = false;

	_error = acquire_device();
	if (_error != LIBUSB_SUCCESS) {
		return;
	}

	if (!_dev_handle) { // can this actually happen?
		_error = -1;
		return;
	}

	_state.shuttle = 0;
	_state.jog = 0;
	_state.buttons = 0;

	Glib::RefPtr<Glib::IdleSource> source = Glib::IdleSource::create ();
	source->connect (sigc::mem_fun (*this, &ContourDesignControlProtocol::wait_for_event));
	source->attach (_main_loop->get_context ());

	_io_source = source->gobj ();
	g_source_ref (_io_source);
}


void
ContourDesignControlProtocol::stop ()
{
	DEBUG_TRACE (DEBUG::ContourDesignControl, "stop()\n");

	_supposed_to_quit = true;

	if (_io_source) {
		g_source_destroy (_io_source);
		g_source_unref (_io_source);
		_io_source = 0;
	}

	if (_dev_handle) {
		release_device ();
	}
}

void
ContourDesignControlProtocol::handle_event () {
	if (_usb_transfer->status == LIBUSB_TRANSFER_TIMED_OUT) {
		goto resubmit;
	}
	if (_usb_transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		DEBUG_TRACE (DEBUG::ContourDesignControl, string_compose("libusb_transfer not completed: %1\n", _usb_transfer->status));
		_error = LIBUSB_ERROR_NO_DEVICE;
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
                        handle_button_release (btn);
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
		DEBUG_TRACE (DEBUG::ContourDesignControl, "failed to resubmit usb transfer after callback\n");
		stop ();
	}
}


boost::shared_ptr<ButtonBase>
ContourDesignControlProtocol::make_button_action (string action_string)
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
ContourDesignControlProtocol::setup_default_button_actions ()
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

const boost::shared_ptr<ButtonBase>
ContourDesignControlProtocol::get_button_action (unsigned int index) const
{
	if (index >= _button_actions.size()) {
		return boost::shared_ptr<ButtonBase>();
	}
	return _button_actions[index];
}

void
ContourDesignControlProtocol::set_button_action (unsigned int index, const boost::shared_ptr<ButtonBase> btn_act)
{
	if (index >= _button_actions.size()) {
		return;
	}
	_button_actions[index] = btn_act;
}


void
ContourDesignControlProtocol::handle_button_press (unsigned short btn)
{
	if (_test_mode) {
		ButtonPress (btn); /* emit signal */
		return;
	}
	if (btn >= _button_actions.size ()) {
		DEBUG_TRACE (DEBUG::ContourDesignControl,
			     string_compose ("ContourDesign button number out of bounds %1, max is %2\n",
					     btn, _button_actions.size ()));
		return;
	}

	_button_actions[btn]->execute ();
}

void
ContourDesignControlProtocol::handle_button_release (unsigned short btn)
{
	if (_test_mode) {
		ButtonRelease (btn); /* emit signal */
	}
}


void
ContourDesignControlProtocol::prev_marker_keep_rolling ()
{
	timepos_t pos = session->locations()->first_mark_before (timepos_t (session->transport_sample()));

	if (pos.positive() || pos.zero()) {
		session->request_locate (pos.samples());
	} else {
		session->goto_start ();
	}
}

void
ContourDesignControlProtocol::next_marker_keep_rolling ()
{
	timepos_t pos = session->locations()->first_mark_after (timepos_t (session->transport_sample()));

	if (pos.positive() || pos.zero()) {
		session->request_locate (pos.samples());
	} else {
		session->goto_end();
	}
}

void
ContourDesignControlProtocol::jog_event_backward ()
{
	DEBUG_TRACE (DEBUG::ContourDesignControl, "jog event backward\n");
	jump_backward (_jog_distance);
}

void
ContourDesignControlProtocol::jog_event_forward ()
{
	DEBUG_TRACE (DEBUG::ContourDesignControl, "jog event forward\n");
	jump_forward (_jog_distance);
}

void
ContourDesignControlProtocol::jump_forward (JumpDistance dist)
{
	LocateTransportDisposition kr = _keep_rolling ? RollIfAppropriate : MustStop;
	switch (dist.unit) {
	case SECONDS: jump_by_seconds (dist.value, kr); break;
	case BEATS: jump_by_beats (dist.value, kr); break;
	case BARS: jump_by_bars (dist.value, kr); break;
	default: break;
	}
}

void ContourDesignControlProtocol::jump_backward (JumpDistance dist)
{
	JumpDistance bw = dist;
	bw.value = -bw.value;
	jump_forward(bw);
}

void
ContourDesignControlProtocol::set_shuttle_speed (unsigned int index, double speed)
{
	if (index >= _shuttle_speeds.size()) {
		return;
	}
	_shuttle_speeds[index] = speed;
}

void
ContourDesignControlProtocol::shuttle_event (int position)
{
	if (abs(position) > num_shuttle_speeds) {
		DEBUG_TRACE (DEBUG::ContourDesignControl, "received invalid shuttle position... ignoring.\n");
		return;
	}

	if (position != 0) {
		if (_shuttle_was_zero) {
			_was_rolling_before_shuttle = transport_rolling ();
		}
		const vector<double>& spds = _shuttle_speeds;
		const double speed = position > 0 ? spds[position-1] : -spds[-position-1];
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
	ContourDesignControlProtocol* spc = static_cast<ContourDesignControlProtocol*> (transfer->user_data);
	spc->handle_event();
}
