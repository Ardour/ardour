/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_rt_tasklist_h_
#define _ardour_rt_tasklist_h_

#include <list>
#include <boost/function.hpp>

#include "pbd/semutils.h"
#include "pbd/g_atomic_compat.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/audio_backend.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

class LIBARDOUR_API RTTaskList
{
public:
	RTTaskList ();
	~RTTaskList ();

	// TODO use dedicated allocator of a boost::intrusive::list
	typedef std::list<boost::function<void ()> > TaskList;

	/** process tasks in list in parallel, wait for them to complete */
	void process (TaskList const&);

private:
	GATOMIC_QUAL gint      _threads_active;
	std::vector<pthread_t> _threads;

	void reset_thread_list ();
	void drop_threads ();

	void process_tasklist ();

	static void* _thread_run (void *arg);
	void run ();

	Glib::Threads::Mutex _process_mutex;
	Glib::Threads::Mutex _tasklist_mutex;
	PBD::Semaphore _task_run_sem;
	PBD::Semaphore _task_end_sem;

	TaskList _tasklist;
};

} // namespace ARDOUR
#endif
