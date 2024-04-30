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

#ifdef HAVE_IOPRIO
#include <sys/syscall.h>
#endif

#include "pbd/compose.h"
#include "pbd/cpus.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"

#include "temporal/tempo.h"

#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/io_tasklist.h"
#include "ardour/process_thread.h"
#include "ardour/session_event.h"

using namespace ARDOUR;

IOTaskList::IOTaskList (uint32_t n_threads)
	: _n_threads (n_threads)
	, _terminate (false)
	, _exec_sem ("io thread exec", 0)
	, _idle_sem ("io thread idle", 0)
{
	assert (n_threads <= hardware_concurrency ());

	if (n_threads < 2) {
		return;
	}

	pthread_attr_t     attr;
	struct sched_param parm;
	parm.sched_priority = pbd_absolute_rt_priority (SCHED_RR, pbd_pthread_priority (THREAD_IO));

  pthread_attr_init (&attr);
#ifdef PLATFORM_WINDOWS
  pthread_attr_setschedpolicy (&attr, SCHED_OTHER);
#else
  pthread_attr_setschedpolicy (&attr, SCHED_RR);
#endif
  pthread_attr_setschedparam (&attr, &parm);
  pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setinheritsched (&attr, PTHREAD_EXPLICIT_SCHED);

	DEBUG_TRACE (PBD::DEBUG::IOTaskList, string_compose ("IOTaskList starting %1 threads with priority = %2\n", _n_threads, parm.sched_priority));

	_workers.resize (_n_threads);
	for (uint32_t i = 0; i < _n_threads; ++i) {
		if (pthread_create (&_workers[i], &attr, &_worker_thread, this)) {
			throw failed_constructor ();
		}
	}
	pthread_attr_destroy (&attr);
}

IOTaskList::~IOTaskList ()
{
	_terminate.store (true);
	for (size_t i = 0; i < _workers.size (); ++i) {
		_exec_sem.signal ();
	}
	for (auto const& t : _workers) {
		pthread_join (t, NULL);
	}
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
	if (_n_threads > 1 && _tasks.size () > 2) {
		uint32_t wakeup = std::min<uint32_t> (_n_threads, _tasks.size ());
		DEBUG_TRACE (PBD::DEBUG::IOTaskList, string_compose ("IOTaskList process wakeup %1 thread for %2 tasks.\n", wakeup, _tasks.size ()))
		for (uint32_t i = 0; i < wakeup; ++i) {
			_exec_sem.signal ();
		}
		for (uint32_t i = 0; i < wakeup; ++i) {
			_idle_sem.wait ();
		}
	} else {
		DEBUG_TRACE (PBD::DEBUG::IOTaskList, string_compose ("IOTaskList process %1 task(s) in main thread.\n", _tasks.size ()))
		for (auto const& fn : _tasks) {
			fn ();
		}
	}
	_tasks.clear ();
}

void*
IOTaskList::_worker_thread (void* me)
{
	IOTaskList* self = static_cast<IOTaskList*> (me);

	uint32_t id = self->_n_workers.fetch_add (1);
	char name[64];
	snprintf (name, 64, "IO-%u-%p", id, (void*)DEBUG_THREAD_SELF);
	pthread_set_name (name);

	SessionEvent::create_per_thread_pool (name, 64);
	PBD::notify_event_loops_about_thread_creation (pthread_self (), name, 64);

	DiskReader::allocate_working_buffers ();
	ARDOUR::ProcessThread* pt = new ProcessThread ();
	pt->get_buffers ();

#ifdef HAVE_IOPRIO
	/* compare to Butler::_thread_work */
  // ioprio_set (IOPRIO_WHO_PROCESS, 0 /*calling thread*/, IOPRIO_PRIO_VALUE (IOPRIO_CLASS_RT, 4))
  syscall (SYS_ioprio_set, 1, 0, (1 << 13) | 4);
#endif

	self->io_thread ();

	DiskReader::free_working_buffers ();
	pt->drop_buffers ();
	delete pt;
	return 0;
}

void
IOTaskList::io_thread ()
{
	while (1) {
		_exec_sem.wait ();
		if (_terminate.load ()) {
			break;
		}

		Temporal::TempoMap::fetch ();

		while (1) {
			boost::function<void()> fn;
			Glib::Threads::Mutex::Lock lm (_tasks_mutex);
			if (_tasks.empty ()) {
				break;
			}
			fn = _tasks.back ();
			_tasks.pop_back ();
			lm.release ();

			fn ();
		}
		_idle_sem.signal ();
	}
}
