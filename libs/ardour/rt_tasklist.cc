/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#include <cstring>

#include "pbd/g_atomic_compat.h"
#include "pbd/pthread_utils.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/rt_tasklist.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

RTTaskList::RTTaskList ()
	: _task_run_sem ("rt_task_run", 0)
	, _task_end_sem ("rt_task_done", 0)
	, _n_tasks (0)
	, _m_tasks (0)
	, _tasks (256)
{
	g_atomic_int_set (&_threads_active, 0);
	reset_thread_list ();
}

RTTaskList::~RTTaskList ()
{
	drop_threads ();
}

void
RTTaskList::drop_threads ()
{
	g_atomic_int_set (&_threads_active, 0);

	uint32_t nt = _threads.size ();
	for (uint32_t i = 0; i < nt; ++i) {
		_task_run_sem.signal ();
	}
	for (auto const& i : _threads) {
		pthread_join (i, NULL);
	}
	_threads.clear ();
	_task_run_sem.reset ();
	_task_end_sem.reset ();
}

/*static*/ void*
RTTaskList::_thread_run (void* arg)
{
	RTTaskList* d = static_cast<RTTaskList*> (arg);

	char name[64];
	snprintf (name, 64, "RTTask-%p", (void*)DEBUG_THREAD_SELF);
	pthread_set_name (name);

	d->run ();
	pthread_exit (0);
	return 0;
}

void
RTTaskList::reset_thread_list ()
{
	drop_threads ();

	const uint32_t num_threads = how_many_dsp_threads ();
	if (num_threads < 2) {
		return;
	}

	g_atomic_int_set (&_threads_active, 1);
	for (uint32_t i = 0; i < num_threads; ++i) {
		int       rv = 1;
		pthread_t thread_id;
		if (AudioEngine::instance ()->is_realtime ()) {
			rv = pbd_realtime_pthread_create (PBD_SCHED_FIFO, AudioEngine::instance ()->client_real_time_priority (), PBD_RT_STACKSIZE_HELP, &thread_id, _thread_run, this);
		}
		if (rv) {
			rv = pbd_pthread_create (PBD_RT_STACKSIZE_HELP, &thread_id, _thread_run, this);
		}
		if (rv) {
			PBD::fatal << _("Cannot create thread for TaskList!") << " (" << strerror (rv) << ")" << endmsg;
			/* NOT REACHED */
		}
		pbd_mach_set_realtime_policy (thread_id, 5. * 1e-5, false);
		_threads.push_back (thread_id);
	}
}

void
RTTaskList::run ()
{
	bool wait = true;

	while (true) {
		if (wait) {
			_task_run_sem.wait ();
		}

		if (0 == g_atomic_int_get (&_threads_active)) {
			_task_end_sem.signal ();
			break;
		}

		wait = false;

		boost::function<void ()> to_run;
		if (_tasks.pop_front (to_run)) {
			to_run ();
			continue;
		}

		if (!wait) {
			_task_end_sem.signal ();
		}

		wait = true;
	}
}

void
RTTaskList::push_back (boost::function<void ()> fn)
{
	if (!_tasks.push_back (fn)) {
		fn ();
	} else {
		++_n_tasks;
	}
	++_m_tasks;
}

void
RTTaskList::process ()
{
	if (0 == g_atomic_int_get (&_threads_active) || _threads.size () == 0) {
		boost::function<void ()> to_run;
		while (_tasks.pop_front (to_run)) {
			to_run ();
			--_n_tasks;
		}
		assert (_n_tasks == 0);
		_n_tasks = 0;
		return;
	}

	uint32_t nt = std::min (_threads.size (), _n_tasks);

	for (uint32_t i = 0; i < nt; ++i) {
		_task_run_sem.signal ();
	}
	for (uint32_t i = 0; i < nt; ++i) {
		_task_end_sem.wait ();
	}

	/* re-allocate queue if needed */
	if (_tasks.capacity () < _m_tasks) {
		_tasks.reserve (_m_tasks);
	}
	_n_tasks = 0;
	_m_tasks = 0;
}
