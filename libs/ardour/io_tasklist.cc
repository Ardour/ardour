/*
 * Copyright (C) 2017-2024 Robin Gareus <robin@gareus.org>
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

#include "pbd/pthread_utils.h"

#include "ardour/io_tasklist.h"

using namespace ARDOUR;

IOTaskList::IOTaskList ()
	: _n_threads (0)
{
}

IOTaskList::~IOTaskList ()
{
}

void
IOTaskList::push_back (boost::function<void ()> fn)
{
	_tasks.push_back (fn);
}

void
IOTaskList::process ()
{
	assert (strcmp (pthread_name (), "butler") == 0);
	//std::cout << "IOTaskList::process " << pthread_name () << " " << _tasks.size () << "\n";
	if (_n_threads > 1 && _tasks.size () > 2) {
		// TODO
	} else {
		for (auto const& fn : _tasks) {
			fn ();
		}
	}
	_tasks.clear ();
}
