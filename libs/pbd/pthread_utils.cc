/*
 * Copyright (C) 2002-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2024 Robin Gareus <robin@gareus.org>
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
#include <set>
#include <stdint.h>
#include <string>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

#if !defined PLATFORM_WINDOWS && defined __GLIBC__
#include <climits>
#include <dlfcn.h>
#endif

#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"

#ifdef COMPILER_MSVC
DECLARE_DEFAULT_COMPARISONS (pthread_t) // Needed for 'DECLARE_DEFAULT_COMPARISONS'. Objects in an STL container can be
                                        // searched and sorted. Thus, when instantiating the container, MSVC complains
                                        // if the type of object being contained has no appropriate comparison operators
                                        // defined (specifically, if operators '<' and '==' are undefined). This seems
                                        // to be the case with ptw32 'pthread_t' which is a simple struct.

#define pthread_gethandle  pthread_getw32threadhandle_np
#endif

#ifdef __APPLE__
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#endif

using namespace std;

typedef std::map<pthread_t, std::string> ThreadMap;
static ThreadMap                         all_threads;
static pthread_mutex_t                   thread_map_lock = PTHREAD_MUTEX_INITIALIZER;
static Glib::Threads::Private<char>      thread_name (free);
static int                               base_priority_relative_to_max = -20;

#ifdef PLATFORM_WINDOWS
static
std::string GetLastErrorAsString()
{
	DWORD err = ::GetLastError();
	if(err == 0) {
		return std::string ();
	}

	LPSTR buf   = nullptr;
	size_t size = FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                              NULL, err, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);

	std::string rv (buf, size);
	LocalFree (buf);
	return rv;
}

static bool
set_win_set_realtime_policy (pthread_t thread, int priority)
{
	if (priority < 12) {
		return false;
	}
	bool ok = false;

	if (SetPriorityClass (GetCurrentProcess (), 0x00000100 /* REALTIME_PRIORITY_CLASS */)) {
		/* see https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities */
		ok = SetThreadPriority (pthread_gethandle (thread), priority);
		DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Using Windows RT thread class. set priority: %1\n", ok ? "OK" : GetLastErrorAsString ()));
	} else {
		DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Cannot use Windows RT thread class: %1\n", GetLastErrorAsString ()));
		ok = SetPriorityClass (GetCurrentProcess (), 0x00000080 /* HIGH_PRIORITY_CLASS */);
		DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Using Windows high priority thread class: %1\n", ok ? "OK" : GetLastErrorAsString ()));
		if (ok) {
			ok = SetThreadPriority (pthread_gethandle (thread), priority);
			DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Set Windows high thread priority: %1\n", ok ? "OK" : GetLastErrorAsString ()));
		}
	}
	return ok;
}
#endif

namespace PBD
{
	PBD::Signal<void(pthread_t, std::string, uint32_t)> ThreadCreatedWithRequestSize;
}

using namespace PBD;

void
PBD::notify_event_loops_about_thread_creation (pthread_t thread, const std::string& emitting_thread_name, int request_count)
{
	/* notify threads that may exist in the future (they may also exist
	 * already, in which case they will catch the
	 * ThreadCreatedWithRequestSize signal)
	 */
	EventLoop::pre_register (emitting_thread_name, request_count);

	/* notify all existing threads */
	ThreadCreatedWithRequestSize (thread, emitting_thread_name, request_count);
}

static size_t
pbd_stack_size ()
{
	size_t rv = 0;
#if !defined PLATFORM_WINDOWS && defined __GLIBC__

	size_t pt_min_stack = 16384;

#ifdef PTHREAD_STACK_MIN
	pt_min_stack = PTHREAD_STACK_MIN;
#endif

	void* handle = dlopen (NULL, RTLD_LAZY);

	/* This function is internal (it has a GLIBC_PRIVATE) version, but
	 * available via weak symbol, or dlsym, and returns
	 *
	 * GLRO(dl_pagesize) + __static_tls_size + PTHREAD_STACK_MIN
	 */

	size_t (*__pthread_get_minstack) (const pthread_attr_t* attr) =
	    (size_t (*) (const pthread_attr_t*))dlsym (handle, "__pthread_get_minstack");

	if (__pthread_get_minstack != NULL) {
		pthread_attr_t attr;
		pthread_attr_init (&attr);
		rv = __pthread_get_minstack (&attr);
		assert (rv >= pt_min_stack);
		rv -= pt_min_stack;
		pthread_attr_destroy (&attr);
	}
	dlclose (handle);
#endif
	return rv;
}

struct ThreadStartWithName {
	void* (*thread_work) (void*);
	void*       arg;
	std::string name;

	ThreadStartWithName (void* (*f) (void*), void* a, const std::string& s)
		: thread_work (f)
		, arg (a)
		, name (s)
	{}
};

static void*
fake_thread_start (void* arg)
{
	ThreadStartWithName* ts      = (ThreadStartWithName*)arg;
	void* (*thread_work) (void*) = ts->thread_work;
	void* thread_arg             = ts->arg;

	/* name will be deleted by the default handler for GStaticPrivate, when the thread exits */
	pthread_set_name (ts->name.c_str ());

	DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Started: '%1'\n", ts->name));

	/* we don't need this object anymore */
	delete ts;

	/* actually run the thread's work function */
	void* ret = thread_work (thread_arg);

	/* cleanup */
	pthread_mutex_lock (&thread_map_lock);

	for (auto const& t : all_threads) {
		if (pthread_equal (t.first, pthread_self ())) {
			DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Terminated: '%1'\n", t.second));
			all_threads.erase (t.first);
			break;
		}
	}
	pthread_mutex_unlock (&thread_map_lock);

	/* done */
	return ret;
}

int
pthread_create_and_store (string name, pthread_t* thread, void* (*start_routine) (void*), void* arg, uint32_t stacklimit)
{
	pthread_attr_t default_attr;
	int            ret;

	/* set default stack size to sensible default for memlocking */
	pthread_attr_init (&default_attr);
	if (stacklimit > 0) {
		pthread_attr_setstacksize (&default_attr, stacklimit + pbd_stack_size ());
	}

	ThreadStartWithName* ts = new ThreadStartWithName (start_routine, arg, name);

	if ((ret = pthread_create (thread, &default_attr, fake_thread_start, ts)) == 0) {
		pthread_mutex_lock (&thread_map_lock);
		all_threads[*thread] = name;
		pthread_mutex_unlock (&thread_map_lock);
	}

	pthread_attr_destroy (&default_attr);

	return ret;
}

void
pthread_set_name (const char* str)
{
	/* copy string and delete it when exiting */
	thread_name.set (strdup (str)); // leaks

#if !defined PTW32_VERSION && defined _GNU_SOURCE
	/* set public thread name, up to 16 chars */
	char ptn[16];
	memset (ptn, 0, 16);
	strncpy (ptn, str, 15);
	pthread_setname_np (pthread_self (), ptn);
#endif
}

const char*
pthread_name ()
{
	const char* str = thread_name.get ();

	if (str) {
		return str;
	}
	return "unknown";
}

void
pthread_kill_all (int signum)
{
	pthread_mutex_lock (&thread_map_lock);
	for (auto const& t : all_threads) {
		if (pthread_equal (t.first, pthread_self ())) {
			continue;
		}
		DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Kill: '%1'\n", t.second));
		pthread_kill (t.first, signum);
	}
	all_threads.clear ();
	pthread_mutex_unlock (&thread_map_lock);
}

void
pthread_cancel_all ()
{
	pthread_mutex_lock (&thread_map_lock);
	for (auto const& t : all_threads) {
		if (pthread_equal (t.first, pthread_self ())) {
			continue;
		}
		DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Cancel: '%1'\n", t.second));
		pthread_cancel (t.first);
	}
	all_threads.clear ();
	pthread_mutex_unlock (&thread_map_lock);
}

void
pthread_cancel_one (pthread_t thread)
{
	pthread_mutex_lock (&thread_map_lock);
	for (auto const& t : all_threads) {
		if (pthread_equal (t.first, thread)) {
			all_threads.erase (t.first);
			break;
		}
	}

	pthread_cancel (thread);
	pthread_mutex_unlock (&thread_map_lock);
}

int
pbd_pthread_create (
		const size_t stacksize,
		pthread_t*   thread,
		void* (*start_routine) (void*),
		void* arg)
{
	int rv;

	pthread_attr_t attr;
	pthread_attr_init (&attr);
	if (stacksize > 0) {
		pthread_attr_setstacksize (&attr, stacksize + pbd_stack_size ());
	}
	DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Start Non-RT Thread stacksize = 0x%1%2\n", std::hex, stacksize));
	rv = pthread_create (thread, &attr, start_routine, arg);
	pthread_attr_destroy (&attr);
	return rv;
}

void
pbd_set_engine_rt_priority (int p)
{
	/* this is mainly for JACK's benefit */
	const int p_max = sched_get_priority_max (SCHED_FIFO);
	const int p_min = sched_get_priority_min (SCHED_FIFO);
	if (p <= 0 || p <= p_min + 10 || p > p_max) {
		base_priority_relative_to_max = -20;
	} else {
		base_priority_relative_to_max =  p - p_max;
	}
}

int
pbd_pthread_priority (PBDThreadClass which)
{
	/* fall back to use values relative to max */
#ifdef PLATFORM_WINDOWS
	switch (which) {
		case THREAD_MAIN:
			return -1; // THREAD_PRIORITY_TIME_CRITICAL (15)
		case THREAD_MIDI:
		case THREAD_PROC:
		case THREAD_CTRL:
		default:
			return -14;  // THREAD_PRIORITY_HIGHEST (2)
		case THREAD_IOFX:
			/* https://github.com/mingw-w64/mingw-w64/blob/master/mingw-w64-libraries/winpthreads/src/sched.c */
			return -15; // THREAD_PRIORITY_ABOVE_NORMAL (1)
	}
#else
	int base = base_priority_relative_to_max;
	const char* p = getenv ("ARDOUR_SCHED_PRI");
	if (p && *p) {
		base = atoi (p);
		if (base > -5 || base < -85) {
			base = -20;
		}
	}

	switch (which) {
		case THREAD_MAIN:
			return base;
		case THREAD_MIDI:
			return base - 1;
		default:
		case THREAD_PROC:
			return base - 2;
		case THREAD_CTRL:
			return base - 3;
		case THREAD_IOFX:
			return base - 10;
	}
#endif
}

int
pbd_absolute_rt_priority (int policy, int priority)
{
	/* POSIX requires a spread of at least 32 steps between min..max */
	const int p_min = sched_get_priority_min (policy); // Linux: 1   Windows -15
	const int p_max = sched_get_priority_max (policy); // Linux: 99  Windows +15

	/* priority is relative to the max */
	assert (priority < 0);
	priority += p_max + 1;

	priority = std::min (p_max, priority);
	priority = std::max (p_min, priority);
	return priority;
}

int
pbd_realtime_pthread_create (
		std::string const& debug_name,
		const int policy, int priority, const size_t stacksize,
		pthread_t* thread,
		void* (*start_routine) (void*),
		void* arg)
{
	int rv;

	pthread_attr_t     attr;
	struct sched_param parm;

	parm.sched_priority = pbd_absolute_rt_priority (policy, priority);

	pthread_attr_init (&attr);
	pthread_attr_setschedpolicy (&attr, policy);
	pthread_attr_setschedparam (&attr, &parm);
	pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setinheritsched (&attr, PTHREAD_EXPLICIT_SCHED);
	if (stacksize > 0) {
		pthread_attr_setstacksize (&attr, stacksize + pbd_stack_size ());
	}
	DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Start RT Thread: '%1' policy = %2 priority = %3 stacksize = 0x%4%5\n", debug_name, policy, parm.sched_priority, std::hex, stacksize));
	rv = pthread_create (thread, &attr, start_routine, arg);
	pthread_attr_destroy (&attr);

#ifdef PLATFORM_WINDOWS
	if (0 == rv && thread && parm.sched_priority >= 12) {
		set_win_set_realtime_policy (*thread, parm.sched_priority);
	}
#endif
	return rv;
}

int
pbd_set_thread_priority (pthread_t thread, int policy, int priority)
{
#if defined PLATFORM_WINDOWS
	policy = SCHED_OTHER;
#endif

	struct sched_param param;
	memset (&param, 0, sizeof (param));
	param.sched_priority = pbd_absolute_rt_priority (policy, priority);

	DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Change '%1' to policy = %2 priority = %3\n", pthread_name(), policy, param.sched_priority));

#ifdef PLATFORM_WINDOWS
	if (is_pthread_active (thread) && param.sched_priority >= 12) {
		if (set_win_set_realtime_policy (thread, param.sched_priority)) {
			return 0;
		}
	}
#endif

	return pthread_setschedparam (thread, SCHED_FIFO, &param);
}

bool
pbd_mach_set_realtime_policy (pthread_t thread_id, double period_ns, bool main)
{
#ifdef __APPLE__
	/* https://opensource.apple.com/source/xnu/xnu-4570.61.1/osfmk/mach/thread_policy.h.auto.html
	 * https://opensource.apple.com/source/xnu/xnu-4570.61.1/:sposfmk/kern/sched.h.auto.html
	 */
	kern_return_t res;

	/* Ask for fixed priority */
	thread_extended_policy_data_t tep;
	tep.timeshare = false;

	res = thread_policy_set (pthread_mach_thread_np (thread_id),
	                         THREAD_EXTENDED_POLICY,
	                         (thread_policy_t)&tep,
	                         THREAD_EXTENDED_POLICY_COUNT);
#ifndef NDEBUG
	printf ("Mach Thread(%p) set timeshare: %d OK: %d\n", thread_id, tep.timeshare, res == KERN_SUCCESS);
#endif

	/* relative value of the computation compared to the other threads in the task. */
	thread_precedence_policy_data_t tpp;
	tpp.importance = main ? 63 : 62; // MAXPRI_USER = 63

	res = thread_policy_set (pthread_mach_thread_np (thread_id),
	                         THREAD_PRECEDENCE_POLICY,
	                         (thread_policy_t)&tpp,
	                         THREAD_PRECEDENCE_POLICY_COUNT);
#ifndef NDEBUG
	printf ("Mach Thread(%p) set precedence: %d OK: %d\n", thread_id, tpp.importance, res == KERN_SUCCESS);
#endif

	/* Realtime constraints */
	double ticks_per_ns = 1.;
	mach_timebase_info_data_t timebase;
	if (KERN_SUCCESS == mach_timebase_info (&timebase)) {
		ticks_per_ns = (double)timebase.denom / (double)timebase.numer;
	}

	thread_time_constraint_policy_data_t tcp;
#ifndef NDEBUG
	mach_msg_type_number_t msgt = 4;
	boolean_t              dflt = false;
	kern_return_t          rv   = thread_policy_get (pthread_mach_thread_np (thread_id),
	                                                 THREAD_TIME_CONSTRAINT_POLICY,
	                                                 (thread_policy_t)&tcp,
	                                                 &msgt, &dflt);

	printf ("Mach Thread(%p) get: period=%d comp=%d constraint=%d preemt=%d OK: %d\n", thread_id, tcp.period, tcp.computation, tcp.constraint, tcp.preemptible, rv == KERN_SUCCESS);
#endif

	const double period_clk = period_ns * ticks_per_ns;

	tcp.period      = period_clk;
	tcp.computation = period_clk * .9;
	tcp.constraint  = period_clk * .95;
	tcp.preemptible = true;

#ifndef NDEBUG
	printf ("period_ns=%f period_clk=%f timebase.num=%d timebase_den=%d ticks_per_ns=%f\n", period_ns, period_clk, timebase.numer, timebase.denom, ticks_per_ns);
	printf ("Mach Thread(%p) request: period=%d comp=%d constraint=%d preemt=%d\n", thread_id, tcp.period, tcp.computation, tcp.constraint, tcp.preemptible);
#endif

	res = thread_policy_set (pthread_mach_thread_np (thread_id),
	                         THREAD_TIME_CONSTRAINT_POLICY,
	                         (thread_policy_t)&tcp,
	                         THREAD_TIME_CONSTRAINT_POLICY_COUNT);

#ifndef NDEBUG
	printf ("Mach Thread(%p) set: period=%d comp=%d constraint=%d preemt=%d OK: %d\n", thread_id, tcp.period, tcp.computation, tcp.constraint, tcp.preemptible, res == KERN_SUCCESS);
#endif

	return res != KERN_SUCCESS;
#endif
	return false; // OK
}

PBD::Thread*
PBD::Thread::create (std::function<void ()> const& slot, std::string const& name)
{
	try {
		return new PBD::Thread (slot, name);
	} catch (...) {
		return 0;
	}
}

PBD::Thread*
PBD::Thread::self ()
{
	return new PBD::Thread ();
}

PBD::Thread::Thread ()
	: _name ("Main")
	, _joinable (false)
{
	_t = pthread_self ();
}

PBD::Thread::Thread (std::function<void ()> const& slot, std::string const& name)
	: _name (name)
	, _slot (slot)
	, _joinable (true)
{
	pthread_attr_t thread_attributes;
	pthread_attr_init (&thread_attributes);

	if (pthread_create (&_t, &thread_attributes, _run, this)) {
		throw failed_constructor ();
	}

	if (_joinable) {
		pthread_mutex_lock (&thread_map_lock);
		all_threads[_t] = name;
		pthread_mutex_unlock (&thread_map_lock);
	}
}

void*
PBD::Thread::_run (void* arg) {
	PBD::Thread* self = static_cast<PBD::Thread *>(arg);
	if (!self->_name.empty ()) {
		pthread_set_name (self->_name.c_str ());
	}

	DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Started: '%1'\n", self->_name));

	self->_slot ();

	/* cleanup */
	pthread_mutex_lock (&thread_map_lock);
	for (auto const& t : all_threads) {
		if (pthread_equal (t.first, pthread_self ())) {
			DEBUG_TRACE (PBD::DEBUG::Threads, string_compose ("Terminated: '%1'\n", t.second));
			all_threads.erase (t.first);
			break;
		}
	}
	pthread_mutex_unlock (&thread_map_lock);

	pthread_exit (0);
	return 0;
}

void
PBD::Thread::join ()
{
	if (_joinable) {
		pthread_join (_t, NULL);
	}
}

bool
PBD::Thread::caller_is_self () const
{
	return pthread_equal (_t, pthread_self ()) != 0;
}
