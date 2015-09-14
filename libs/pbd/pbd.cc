/*
    Copyright (C) 2011 Paul Davis

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

#ifdef PLATFORM_WINDOWS
#include <winsock2.h>
#include "pbd/windows_timer_utils.h"
#include "pbd/windows_mmcss.h"
#endif

#include "i18n.h"

extern void setup_libpbd_enums ();

namespace {

static bool libpbd_initialized = false;

}

void
set_debug_options_from_env ()
{
	bool set;
	std::string options;

	options = Glib::getenv ("PBD_DEBUG", set);
	if (set) {
		std::cerr << X_("PBD_DEBUG=") << options << std::endl;
		PBD::parse_debug_options (options.c_str());
	}
}

#ifdef PLATFORM_WINDOWS
void
test_timers_from_env ()
{
	bool set;
	std::string options;

	options = Glib::getenv ("PBD_TEST_TIMERS", set);
	if (set) {
		if (!PBD::QPC::check_timer_valid ()) {
			PBD::error << X_("Windows QPC Timer source not usable") << endmsg;
		} else {
			PBD::info << X_("Windows QPC Timer source usable") << endmsg;
		}
	}
}
#endif

bool
PBD::init ()
{
	if (libpbd_initialized) {
		return true;
	}

#ifdef PLATFORM_WINDOWS
	// Essential!!  Make sure that any files used by Ardour
	//              will be created or opened in BINARY mode!
	_fmode = O_BINARY;

	WSADATA	wsaData;

	/* Initialize windows socket DLL for PBD::CrossThreadChannel
	 */
	
	if (WSAStartup(MAKEWORD(1,1),&wsaData) != 0) {
		fatal << X_("Windows socket initialization failed with error: ") << WSAGetLastError() << endmsg;
		abort();
		/*NOTREACHED*/
		return false;
	}

	test_timers_from_env ();

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

	set_debug_options_from_env ();

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
