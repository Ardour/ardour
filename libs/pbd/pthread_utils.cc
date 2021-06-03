/*
 * Copyright (C) 2002-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
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

#if !defined PLATFORM_WINDOWS && defined __GLIBC__
#include <climits>
#include <dlfcn.h>
#endif

#include "pbd/pthread_utils.h"

#ifdef COMPILER_MSVC
DECLARE_DEFAULT_COMPARISONS (pthread_t) // Needed for 'DECLARE_DEFAULT_COMPARISONS'. Objects in an STL container can be
                                        // searched and sorted. Thus, when instantiating the container, MSVC complains
                                        // if the type of object being contained has no appropriate comparison operators
                                        // defined (specifically, if operators '<' and '==' are undefined). This seems
                                        // to be the case with ptw32 'pthread_t' which is a simple struct.
#endif

#ifdef __APPLE__
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#endif

using namespace std;

typedef std::list<pthread_t>        ThreadMap;
static ThreadMap                    all_threads;
static pthread_mutex_t              thread_map_lock = PTHREAD_MUTEX_INITIALIZER;
static Glib::Threads::Private<char> thread_name (free);

namespace PBD
{
	PBD::Signal3<void, pthread_t, std::string, uint32_t> ThreadCreatedWithRequestSize;
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

	/* we don't need this object anymore */
	delete ts;

	/* actually run the thread's work function */
	void* ret = thread_work (thread_arg);

	/* cleanup */
	pthread_mutex_lock (&thread_map_lock);

	for (ThreadMap::iterator i = all_threads.begin (); i != all_threads.end (); ++i) {
		if (pthread_equal ((*i), pthread_self ())) {
			all_threads.erase (i);
			break;
		}
	}

	pthread_mutex_unlock (&thread_map_lock);

	/* done */
	return ret;
}

int
pthread_create_and_store (string name, pthread_t* thread, void* (*start_routine) (void*), void* arg)
{
	pthread_attr_t default_attr;
	int            ret;

	/* set default stack size to sensible default for memlocking */
	pthread_attr_init (&default_attr);
	pthread_attr_setstacksize (&default_attr, 0x80000); // 512kB

	ThreadStartWithName* ts = new ThreadStartWithName (start_routine, arg, name);

	if ((ret = pthread_create (thread, &default_attr, fake_thread_start, ts)) == 0) {
		pthread_mutex_lock (&thread_map_lock);
		all_threads.push_back (*thread);
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

#if !defined PLATFORM_WINDOWS && defined _GNU_SOURCE
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
	for (ThreadMap::iterator i = all_threads.begin (); i != all_threads.end (); ++i) {
		if (!pthread_equal ((*i), pthread_self ())) {
			pthread_kill ((*i), signum);
		}
	}
	all_threads.clear ();
	pthread_mutex_unlock (&thread_map_lock);
}

void
pthread_cancel_all ()
{
	pthread_mutex_lock (&thread_map_lock);

	for (ThreadMap::iterator i = all_threads.begin (); i != all_threads.end ();) {
		ThreadMap::iterator nxt = i;
		++nxt;

		if (!pthread_equal ((*i), pthread_self ())) {
			pthread_cancel ((*i));
		}

		i = nxt;
	}
	all_threads.clear ();
	pthread_mutex_unlock (&thread_map_lock);
}

void
pthread_cancel_one (pthread_t thread)
{
	pthread_mutex_lock (&thread_map_lock);
	for (ThreadMap::iterator i = all_threads.begin (); i != all_threads.end (); ++i) {
		if (pthread_equal ((*i), thread)) {
			all_threads.erase (i);
			break;
		}
	}

	pthread_cancel (thread);
	pthread_mutex_unlock (&thread_map_lock);
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
	pthread_attr_setstacksize (&attr, stacksize + pbd_stack_size ());
	rv = pthread_create (thread, &attr, start_routine, arg);
	pthread_attr_destroy (&attr);
	return rv;
}

int
pbd_pthread_priority (PBDThreadClass which)
{
	/* fall back to use values relative to max */
#ifdef PLATFORM_WINDOWS
	switch (which) {
		case THREAD_MAIN:
			return -1;
		case THREAD_MIDI:
			return -2;
		default:
		case THREAD_PROC:
			return -2;
	}
#else
	int base = -20;
	const char* p = getenv ("ARDOUR_SCHED_PRI");
	if (p && *p) {
		base = atoi (p);
		if (base > -5 && base < 5) {
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
	}
#endif
}

int
pbd_absolute_rt_priority (int policy, int priority)
{
	/* POSIX requires a spread of at least 32 steps between min..max */
	const int p_min = sched_get_priority_min (policy); // Linux: 1
	const int p_max = sched_get_priority_max (policy); // Linux: 99

	if (priority == 0) {
		assert (0);
		priority = (p_min + p_max) / 2;
	} else if (priority > 0) {
		/* value relative to minium */
		priority += p_min - 1;
	} else {
		/* value relative maximum */
		priority += p_max + 1;
	}

	if (priority > p_max) {
		priority = p_max;
	}
	if (priority < p_min) {
		priority = p_min;
	}
	return priority;
}

int
pbd_realtime_pthread_create (
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
	pthread_attr_setstacksize (&attr, stacksize + pbd_stack_size ());
	rv = pthread_create (thread, &attr, start_routine, arg);
	pthread_attr_destroy (&attr);
	return rv;
}

int
pbd_set_thread_priority (pthread_t thread, const int policy, int priority)
{
	struct sched_param param;
	memset (&param, 0, sizeof (param));
	param.sched_priority = pbd_absolute_rt_priority (policy, priority);

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
		ticks_per_ns = timebase.denom / timebase.numer;
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

	mach_timebase_info_data_t timebase_info;
	mach_timebase_info (&timebase_info);
	const double period_clk = period_ns * (double)timebase_info.denom / (double)timebase_info.numer;

	tcp.period      = ticks_per_ns * period_clk;
	tcp.computation = ticks_per_ns * period_clk * .9;
	tcp.constraint  = ticks_per_ns * period_clk * .95;
	tcp.preemptible = true;

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
