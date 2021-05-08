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
	Glib::Threads::Mutex::Lock pm (_process_mutex);
	g_atomic_int_set (&_threads_active, 0);

	uint32_t nt = _threads.size ();
	for (uint32_t i = 0; i < nt; ++i) {
		_task_run_sem.signal ();
	}
	for (std::vector<pthread_t>::const_iterator i = _threads.begin (); i != _threads.end (); ++i) {
		pthread_join (*i, NULL);
	}
	_threads.clear ();
	_task_run_sem.reset ();
	_task_end_sem.reset ();
}

/*static*/ void*
RTTaskList::_thread_run (void *arg)
{
	RTTaskList *d = static_cast<RTTaskList *>(arg);
	pthread_set_name ("RTTaskList");
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

	Glib::Threads::Mutex::Lock pm (_process_mutex);

	g_atomic_int_set (&_threads_active, 1);
	for (uint32_t i = 0; i < num_threads; ++i) {
		pthread_t thread_id;
		int rv = 1;
		if (AudioEngine::instance()->is_realtime ()) {
			rv = pbd_realtime_pthread_create (PBD_SCHED_FIFO, AudioEngine::instance()->client_real_time_priority(), PBD_RT_STACKSIZE_HELP, &thread_id, _thread_run, this);
		}
		if (rv) {
			rv = pbd_pthread_create (PBD_RT_STACKSIZE_HELP, &thread_id, _thread_run, this);
		}
		if (rv) {
			PBD::fatal << _("Cannot create thread for TaskList!") << " (" << strerror(rv) << ")" << endmsg;
			/* NOT REACHED */
		}
		pbd_mach_set_realtime_policy (thread_id, 5. * 1e-5, false);
		_threads.push_back (thread_id);
	}
}

void
RTTaskList::run ()
{
	Glib::Threads::Mutex::Lock tm (_tasklist_mutex, Glib::Threads::NOT_LOCK);
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
		tm.acquire ();
		if (!_tasklist.empty ()) {
			to_run = _tasklist.front();
			_tasklist.pop_front ();
		}
		tm.release ();

		if (!to_run.empty ()) {
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
RTTaskList::process (TaskList const& tl)
{
	Glib::Threads::Mutex::Lock pm (_process_mutex);
	Glib::Threads::Mutex::Lock tm (_tasklist_mutex, Glib::Threads::NOT_LOCK);

	tm.acquire ();
	_tasklist = tl;
	tm.release ();

	process_tasklist ();

	tm.acquire ();
	_tasklist.clear ();
	tm.release ();
}

void
RTTaskList::process_tasklist ()
{
//	if (0 == g_atomic_int_get (&_threads_active) || _threads.size () == 0) {

		for (TaskList::iterator i = _tasklist.begin (); i != _tasklist.end(); ++i) {
			(*i)();
		}
		return;
//	}

	uint32_t nt = std::min (_threads.size (), _tasklist.size ());

	for (uint32_t i = 0; i < nt; ++i) {
		_task_run_sem.signal ();
	}
	for (uint32_t i = 0; i < nt; ++i) {
		_task_end_sem.wait ();
	}
}
