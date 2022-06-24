/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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
#include <thread>

#include "common.h"

#include "pbd/error.h"
#include "pbd/semutils.h"

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

PBD::Semaphore sem ("sync", 0);

void worker (int i)
{
	sem.wait ();
	PBD::error << "Thread: " << i << endmsg;
}

int main (int argc, char* argv[])
{
	SessionUtils::init();

	int n_thread = 0;

	if (argc > 1) {
		n_thread = atoi (argv[1]);
	}

	if (n_thread < 1 || n_thread > 512) {
		n_thread = 16;
	}

	printf ("Starting %d threads\n", n_thread);

	std::vector<std::thread> threads;
	for (int c = 0; c < n_thread; ++c) {
		threads.push_back (std::thread (worker, c + 1));
	}

	for (size_t c = 0; c < threads.size (); ++c) {
		sem.signal ();
	}

	for (auto& th : threads) {
		th.join ();
	}

	SessionUtils::cleanup();
	return 0;
}
