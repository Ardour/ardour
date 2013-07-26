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

#include <giomm.h>

#include <glibmm/thread.h>

#include "pbd/pbd.h"
#include "pbd/debug.h"
#include "pbd/id.h"
#include "pbd/enumwriter.h"

#include "i18n.h"

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
	EnumWriter::destroy ();
}
