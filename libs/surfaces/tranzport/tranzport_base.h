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

/* This header file is basically where all the tranzport debuggable options go.
   Try to only check it in with minimal debugging enabled so production
   systems don't have to fiddle with it. */

/* Design notes: The tranzport is a unique device, basically a
   20x2 character lcd gui with (almost) 22 shift keys and 8 blinking lights.

   As such it has several unique constraints. In the libusb driver,
   the device exerts flow control
   by having a usb write fail. It is pointless to retry madly at that point,
   the device is busy, and it's not going to become unbusy very quickly.

   So writes need to be either "mandatory" or "unreliable", and therein
   lies the rub, as the kernel can also drop writes, and missing an
   interrupt in userspace is also generally bad.

   However, the kernel driver retries writes for you and also buffers and
   compresses incoming wheel events - it will rarely, if ever, drop data.

   A more complex surface might have hundreds of lights and several displays.

   mike@taht.net
*/

#ifndef ardour_tranzport_base
#define ardour_tranzport_base

#define DEFAULT_USB_TIMEOUT 10
#define MAX_RETRY 1
#define MAX_TRANZPORT_INFLIGHT 4
#define DEBUG_TRANZPORT 0

#ifndef HAVE_TRANZPORT_KERNEL_DRIVER
#define HAVE_TRANZPORT_KERNEL_DRIVER 0
#endif

#ifndef HAVE_TRANZPORT_MIDI_DRIVER
#define HAVE_TRANZPORT_MIDI_DRIVER 0
#endif

// for now, this is what the device is called
#define TRANZPORT_DEVICE "/dev/tranzport0"

#if DEBUG_TRANZPORT > 0
#define DEBUG_TRANZPORT_SCREEN 10
#define DEBUG_TRANZPORT_BITS 10
#define DEBUG_TRANZPORT_LIGHTS 10
#define DEBUG_TRANZPORT_STATE 10
#else
#define DEBUG_TRANZPORT 0
#define DEBUG_TRANZPORT_BITS 0
#define DEBUG_TRANZPORT_SCREEN 0
#define DEBUG_TRANZPORT_LIGHTS 0
#define DEBUG_TRANZPORT_STATE 0
#endif
#endif /* ardour_tranzport_base */

