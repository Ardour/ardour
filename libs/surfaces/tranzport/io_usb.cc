/*
 *   Copyright (C) 2006 Paul Davis
 *   Copyright (C) 2007 Michael Taht
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   */

#include <iostream>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>

#include <tranzport_control_protocol.h>

#if !HAVE_TRANZPORT_KERNEL_DRIVER

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace PBD;

#include "pbd/i18n.h"

#include <pbd/abstract_ui.cc>

// I note that these usb specific open, close, probe, read routines are basically
// pure boilerplate and could easily be abstracted elsewhere

bool
TranzportControlProtocol::probe ()
{
	struct usb_bus *bus;
	struct usb_device *dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (bus = usb_busses; bus; bus = bus->next) {

		for(dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == VENDORID && dev->descriptor.idProduct == PRODUCTID) {
				return true;
			}
		}
	}

	return false;
}

int
TranzportControlProtocol::open ()
{
	struct usb_bus *bus;
	struct usb_device *dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (bus = usb_busses; bus; bus = bus->next) {

		for(dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor != VENDORID)
				continue;
			if (dev->descriptor.idProduct != PRODUCTID)
				continue;
			return open_core (dev);
		}
	}

	cerr << _("Tranzport: no device detected") << endmsg;
	return -1;
}

int
TranzportControlProtocol::open_core (struct usb_device* dev)
{
	if (!(udev = usb_open (dev))) {
		cerr << _("Tranzport: cannot open USB transport") << endmsg;
		return -1;
	}

	if (usb_claim_interface (udev, 0) < 0) {
		cerr << _("Tranzport: cannot claim USB interface") << endmsg;
		usb_close (udev);
		udev = 0;
		return -1;
	}

	if (usb_set_configuration (udev, 1) < 0) {
		cerr << _("Tranzport: cannot configure USB interface") << endmsg;
	}

	return 0;
}

int
TranzportControlProtocol::close ()
{
	int ret = 0;

	if (udev == 0) {
		return 0;
	}

	if (usb_release_interface (udev, 0) < 0) {
		cerr << _("Tranzport: cannot release interface") << endmsg;
		ret = -1;
	}

	if (usb_close (udev)) {
		cerr << _("Tranzport: cannot close device") << endmsg;
		udev = 0;
		ret = 0;
	}

	return ret;
}

int TranzportControlProtocol::read(uint8_t *buf, uint32_t timeout_override)
{
	last_read_error = usb_interrupt_read (udev, READ_ENDPOINT, (char *) buf, 8, timeout_override);
	switch(last_read_error) {
	case -ENOENT:
	case -ENXIO:
	case -ECONNRESET:
	case -ESHUTDOWN:
	case -ENODEV:
		cerr << "Tranzport disconnected, errno: " << last_read_error;
		set_active(false);
	case -ETIMEDOUT: // This is normal
		break;
	default:
#if DEBUG_TRANZPORT
		cerr << "Got an unknown error on read:" << last_read_error "\n";
#endif
		break;
	}

	return last_read_error;
}


int
TranzportControlProtocol::write_noretry (uint8_t* cmd, uint32_t timeout_override)
{
	int val;
	if(inflight > MAX_TRANZPORT_INFLIGHT) { return (-1); }
	val = usb_interrupt_write (udev, WRITE_ENDPOINT, (char*) cmd, 8, timeout_override ? timeout_override : timeout);

	if (val < 0 && val !=8) {
#if DEBUG_TRANZPORT
		printf("usb_interrupt_write failed: %d\n", val);
#endif
		last_write_error = val;
		switch(last_write_error) {
		case -ENOENT:
		case -ENXIO:
		case -ECONNRESET:
		case -ESHUTDOWN:
		case -ENODEV:
			cerr << "Tranzport disconnected, errno: " << last_write_error;
			set_active(false);
		case -ETIMEDOUT: // This is normal
			break;
		default:
#if DEBUG_TRANZPORT
			cerr << "Got an unknown error on read:" << last_write_error "\n";
#endif
			break;
		}
		return val;
	}

	last_write_error = 0;
	++inflight;

	return 0;

}

int
TranzportControlProtocol::write (uint8_t* cmd, uint32_t timeout_override)
{
#if MAX_RETRY > 1
	int val;
	int retry = 0;
	if(inflight > MAX_TRANZPORT_INFLIGHT) { return (-1); }

	while((val = usb_interrupt_write (udev, WRITE_ENDPOINT, (char*) cmd, 8, timeout_override ? timeout_override : timeout))!=8 && retry++ < MAX_RETRY) {
		printf("usb_interrupt_write failed, retrying: %d\n", val);
	}

	if (retry == MAX_RETRY) {
		printf("Too many retries on a tranzport write, aborting\n");
	}

	if (val < 0) {
		printf("usb_interrupt_write failed: %d\n", val);
		return val;
	}
	if (val != 8) {
		printf("usb_interrupt_write failed: %d\n", val);
		return -1;
	}
	++inflight;
	return 0;
#else
	return (write_noretry(cmd,timeout_override));
#endif

}

#endif
