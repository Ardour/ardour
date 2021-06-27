/*
 * Copyright (C) 2013-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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
#include <cstdlib>
#include <string>

#ifdef PLATFORM_WINDOWS
#include <fcntl.h>
#endif

#include <giomm.h>

#include <glibmm/thread.h>

#include "pbd/pbd.h"
#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/id.h"
#include "pbd/enumwriter.h"
#include "pbd/fpu.h"
#include "pbd/microseconds.h"
#include "pbd/xml++.h"

#ifdef PLATFORM_WINDOWS
#include <winsock2.h>
#include "pbd/windows_timer_utils.h"
#include "pbd/windows_mmcss.h"
#endif

#include "pbd/i18n.h"

extern void setup_libpbd_enums ();

namespace {

static bool libpbd_initialized = false;

}

bool
PBD::init ()
{
	if (libpbd_initialized) {
		return true;
	}

	microsecond_timer_init ();

#ifdef PLATFORM_WINDOWS
	// Essential!!  Make sure that any files used by Ardour
	//              will be created or opened in BINARY mode!
	_fmode = O_BINARY;

	WSADATA wsaData;

	/* Initialize windows socket DLL for PBD::CrossThreadChannel
	 */

	if (WSAStartup(MAKEWORD(1,1),&wsaData) != 0) {
		error << X_("Windows socket initialization failed with error: ") << WSAGetLastError() << endmsg;
		return false;
	}

	if (!PBD::MMCSS::initialize()) {
		PBD::info << X_("Unable to initialize MMCSS") << endmsg;
	} else {
		PBD::info << X_("MMCSS Initialized") << endmsg;
	}
#endif

	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

	Gio::init ();

	PBD::ID::init ();

	setup_libpbd_enums ();

	libpbd_initialized = true;
	return true;
}

void
PBD::cleanup ()
{
#ifdef PLATFORM_WINDOWS
	PBD::MMCSS::deinitialize ();
	WSACleanup();
#endif

	EnumWriter::destroy ();
	FPU::destroy ();
}
