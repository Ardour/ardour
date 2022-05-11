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

#include <boost/function.hpp>
#include <list>

#include "pbd/g_atomic_compat.h"
#include "pbd/mpmc_queue.h"
#include "pbd/semutils.h"

#include "ardour/audio_backend.h"
#include "ardour/libardour_visibility.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

namespace ARDOUR
{
class LIBARDOUR_API RTTaskList
{
public:
	RTTaskList ();
	~RTTaskList ();

	/** process tasks in list in parallel, wait for them to complete */
	void process ();
	void push_back (boost::function<void ()> fn);

private:
	GATOMIC_QUAL gint      _threads_active;
	std::vector<pthread_t> _threads;

	void reset_thread_list ();
	void drop_threads ();
	void run ();

	static void* _thread_run (void* arg);

	PBD::Semaphore _task_run_sem;
	PBD::Semaphore _task_end_sem;

	size_t _n_tasks;
	size_t _m_tasks;

	PBD::MPMCQueue<boost::function<void ()>> _tasks;
};

} // namespace ARDOUR
#endif
