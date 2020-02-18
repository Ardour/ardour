/*
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#include "common.h"

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

int main (int argc, char* argv[])
{
	SessionUtils::init();
	Session* s = 0;

	s = SessionUtils::load_session (
			"/home/rgareus/Documents/ArdourSessions/TestA/",
			"TestA"
			);

	printf ("SESSION INFO: routes: %lu\n", s->get_routes()->size ());

	sleep(2);

	//s->save_state ("");

	SessionUtils::unload_session(s);
	SessionUtils::cleanup();

	return 0;
}
