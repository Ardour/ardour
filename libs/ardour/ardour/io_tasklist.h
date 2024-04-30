/*
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_io_tasklist_h_
#define _ardour_io_tasklist_h_

#include <atomic>
#include <boost/function.hpp>
#include <vector>
#include <glibmm/threads.h>

#include "pbd/semutils.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR
{

class LIBARDOUR_API IOTaskList
{
public:
	IOTaskList (uint32_t);
	~IOTaskList ();

	/** process tasks in list in parallel, wait for them to complete */
	void process ();
	void push_back (boost::function<void ()> fn);

private:
	static void* _worker_thread (void*);

	void io_thread ();

	std::vector<boost::function<void ()>> _tasks;

	uint32_t               _n_threads;
	std::atomic<uint32_t>  _n_workers;
	std::vector<pthread_t> _workers;
	std::atomic <bool>     _terminate;
	PBD::Semaphore         _exec_sem;
	PBD::Semaphore         _idle_sem;
	Glib::Threads::Mutex   _tasks_mutex;
};

} // namespace ARDOUR
#endif
