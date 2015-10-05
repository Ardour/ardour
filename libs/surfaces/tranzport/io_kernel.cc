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

#if HAVE_TRANZPORT_KERNEL_DRIVER
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include "tranzport_control_protocol.h"

// Something like open(/dev/surface/tranzport/event) for reading and raw for writing) would be better in the long run
// Also support for multiple tranzports needs to be figured out
// And bulk reads/writes in general

bool
TranzportControlProtocol::probe ()
{
	if((udev = ::open(TRANZPORT_DEVICE,O_RDWR))> 0) {
		::close(udev);
		return true;
	}
	error << _("Tranzport: Can't open device for Read/Write: ") << endmsg;
	return false;
}

int
TranzportControlProtocol::open ()
{
	if((udev=::open(TRANZPORT_DEVICE,O_RDWR))> 0) {
		return(udev);
	}
	error << _("Tranzport: no device detected") << endmsg;
	return udev;
}

int
TranzportControlProtocol::close ()
{
	int ret = 0;

	if (udev < 1) {
		return 0;
	}

	if ((ret = ::close (udev)) != 0) {
		error << _("Tranzport: cannot close device") << endmsg;
	}

	return ret;
}

// someday do buffered reads, presently this does blocking reads, which is bad...

int TranzportControlProtocol::read(uint8_t *buf, uint32_t timeout_override)
{
	last_read_error = ::read (udev, (char *) buf, 8);
	switch(errno) {
	case -ENOENT:
	case -ENXIO:
	case -ECONNRESET:
	case -ESHUTDOWN:
	case -ENODEV:
		cerr << "Tranzport disconnected, errno: " << last_read_error;
		set_active(false);
		break;
	case -ETIMEDOUT: // This is not normal, but lets see what happened
		cerr << "Tranzport read timed out, errno: " << last_read_error;
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
	// inflight is now taken care of by the driver, but...
	if(inflight > MAX_TRANZPORT_INFLIGHT) { return (-1); }
	int val = ::write (udev, (char*) cmd, 8);

	if (val < 0 && val !=8) {
#if DEBUG_TRANZPORT
		printf("write failed: %d\n", val);
#endif
		last_write_error = errno;
		switch(last_write_error) {
		case -ENOENT:
		case -ENXIO:
		case -ECONNRESET:
		case -ESHUTDOWN:
		case -ENODEV:
			cerr << "Tranzport disconnected, errno: " << last_write_error;
			set_active(false);
			break;
		case -ETIMEDOUT: // This is not normal but
			cerr << "Tranzport disconnected, errno: " << last_write_error;
			break;
		default:
#if DEBUG_TRANZPORT
			cerr << "Got an unknown error on read:" << last_write_error "\n";
#endif
			break;
		}
		return last_write_error;
	}

	last_write_error = 0;
	++inflight;

	return 0;

}

int
TranzportControlProtocol::write (uint8_t* cmd, uint32_t timeout_override)
{
	return (write_noretry(cmd,timeout_override));
}

// FIXME - install poll semantics
#endif /* HAVE_TRANZPORT_KERNEL_DRIVER */

