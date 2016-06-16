/*
	Copyright (C) 2016 Paul Davis

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "pbd/failed_constructor.h"

#include "push2.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

#include "i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

Push2::Push2 (Session& s)
	: ControlProtocol (s, string (X_("Ableton Push2")))
	, AbstractUI<Push2Request> (name())
	, handle (0)
{
	if ((handle = libusb_open_device_with_vid_pid (NULL, ABLETON, PUSH2)) == 0) {
		throw failed_constructor ();
	}

	libusb_claim_interface (handle, 0x00);
}

Push2::~Push2 ()
{
	if (handle) {
		libusb_release_interface (handle, 0x00);
		libusb_close (handle);
	}
}

bool
Push2::probe ()
{
	libusb_device_handle *h;
	libusb_init (NULL);

	if ((h = libusb_open_device_with_vid_pid (NULL, ABLETON, PUSH2)) == 0) {
		return false;
	}

	libusb_close (h);
	return true;
}

void*
Push2::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	   instantiated in this source module. To provide something visible for
	   use in the interface/descriptor, we have this static method that is
	   template-free.
	*/
	return request_buffer_factory (num_requests);
}

void
Push2::do_request (Push2Request * req)
{
	// DEBUG_TRACE (DEBUG::MackieControl, string_compose ("doing request type %1\n", req->type));
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop ();
	}
}

int
Push2::stop ()
{
	BaseUI::quit ();

	return 0;
}
