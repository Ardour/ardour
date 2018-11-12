/*
    Copyright (C) 2002 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <set>
#include <string>
#include <cstring>
#include <stdint.h>

#include "pbd/pthread_utils.h"
#ifdef WINE_THREAD_SUPPORT
#include <fst.h>
#endif

#ifdef COMPILER_MSVC
DECLARE_DEFAULT_COMPARISONS(pthread_t)  // Needed for 'DECLARE_DEFAULT_COMPARISONS'. Objects in an STL container can be
                                        // searched and sorted. Thus, when instantiating the container, MSVC complains
                                        // if the type of object being contained has no appropriate comparison operators
                                        // defined (specifically, if operators '<' and '==' are undefined). This seems
                                        // to be the case with ptw32 'pthread_t' which is a simple struct.
#endif

using namespace std;

typedef std::list<pthread_t> ThreadMap;
static ThreadMap all_threads;
static pthread_mutex_t thread_map_lock = PTHREAD_MUTEX_INITIALIZER;
static Glib::Threads::Private<char> thread_name (free);

namespace PBD {
	PBD::Signal3<void,pthread_t,std::string,uint32_t> ThreadCreatedWithRequestSize;
}

using namespace PBD;

static int thread_creator (pthread_t* thread_id, const pthread_attr_t* attr, void *(*function)(void*), void* arg)
{
#ifdef WINE_THREAD_SUPPORT
       return wine_pthread_create (thread_id, attr, function, arg);
#else
       return pthread_create (thread_id, attr, function, arg);
#endif
}


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
    void* (*thread_work)(void*);
    void* arg;
    std::string name;

    ThreadStartWithName (void* (*f)(void*), void* a, const std::string& s)
	    : thread_work (f), arg (a), name (s) {}
};

static void*
fake_thread_start (void* arg)
{
	ThreadStartWithName* ts = (ThreadStartWithName*) arg;
	void* (*thread_work)(void*) = ts->thread_work;
	void* thread_arg = ts->arg;

	/* name will be deleted by the default handler for GStaticPrivate, when the thread exits */

	pthread_set_name (ts->name.c_str());

	/* we don't need this object anymore */

	delete ts;

	/* actually run the thread's work function */

	void* ret = thread_work (thread_arg);

	/* cleanup */

	pthread_mutex_lock (&thread_map_lock);

	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ++i) {
		if (pthread_equal ((*i), pthread_self())) {
			all_threads.erase (i);
			break;
		}
	}

	pthread_mutex_unlock (&thread_map_lock);

	/* done */

	return ret;
}

int
pthread_create_and_store (string name, pthread_t  *thread, void * (*start_routine)(void *), void * arg)
{
	pthread_attr_t default_attr;
	int ret;

	// set default stack size to sensible default for memlocking
	pthread_attr_init(&default_attr);
	pthread_attr_setstacksize(&default_attr, 500000);

	ThreadStartWithName* ts = new ThreadStartWithName (start_routine, arg, name);

	if ((ret = thread_creator (thread, &default_attr, fake_thread_start, ts)) == 0) {
		pthread_mutex_lock (&thread_map_lock);
		all_threads.push_back (*thread);
		pthread_mutex_unlock (&thread_map_lock);
	}

	pthread_attr_destroy(&default_attr);

	return ret;
}

void
pthread_set_name (const char *str)
{
	/* copy string and delete it when exiting */

	thread_name.set (strdup (str)); // leaks
}

const char *
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
	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ++i) {
		if (!pthread_equal ((*i), pthread_self())) {
			pthread_kill ((*i), signum);
		}
	}
	all_threads.clear();
	pthread_mutex_unlock (&thread_map_lock);
}

void
pthread_cancel_all ()
{
	pthread_mutex_lock (&thread_map_lock);

	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ) {

		ThreadMap::iterator nxt = i;
		++nxt;

		if (!pthread_equal ((*i), pthread_self())) {
			pthread_cancel ((*i));
		}

		i = nxt;
	}
	all_threads.clear();
	pthread_mutex_unlock (&thread_map_lock);
}

void
pthread_cancel_one (pthread_t thread)
{
	pthread_mutex_lock (&thread_map_lock);
	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ++i) {
		if (pthread_equal ((*i), thread)) {
			all_threads.erase (i);
			break;
		}
	}

	pthread_cancel (thread);
	pthread_mutex_unlock (&thread_map_lock);
}

int
pbd_absolute_rt_priority (int policy, int priority)
{
	/* POSIX requires a spread of at least 32 steps between min..max */
	const int p_min = sched_get_priority_min (policy); // Linux: 1
	const int p_max = sched_get_priority_max (policy); // Linux: 99

	if (priority == 0) {
		/* use default. XXX this should be relative to audio (JACK) thread,
		 * internal backends use -20 (Audio), -21 (MIDI), -22 (compuation)
		 */
		priority = 7; // BaseUI backwards compat.
	}

	if (priority > 0) {
		priority += p_min;
	} else {
		priority += p_max;
	}
	if (priority > p_max) priority = p_max;
	if (priority < p_min) priority = p_min;
	return priority;
}



int
pbd_realtime_pthread_create (
		const int policy, int priority, const size_t stacksize,
		pthread_t *thread,
		void *(*start_routine) (void *),
		void *arg)
{
	int rv;

	pthread_attr_t attr;
	struct sched_param parm;

	parm.sched_priority = pbd_absolute_rt_priority (policy, priority);

	pthread_attr_init (&attr);
	pthread_attr_setschedpolicy (&attr, policy);
	pthread_attr_setschedparam (&attr, &parm);
	pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setinheritsched (&attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setstacksize (&attr, stacksize);
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
pbd_mach_set_realtime_policy (pthread_t thread_id, double period_ns)
{
#ifdef _APPLE_
	thread_time_constraint_policy_data_t policy;
#ifndef NDEBUG
	mach_msg_type_number_t msgt = 4;
	boolean_t dflt = false;
	kern_return_t rv = thread_policy_get (pthread_mach_thread_np (_main_thread),
			THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t) &policy,
			&msgt, &dflt);
	printf ("Mach Thread(%p) %d %d %d %d DFLT %d OK: %d\n", _main_thread, policy.period, policy.computation, policy.constraint, policy.preemptible, dflt, rv == KERN_SUCCESS);
#endif

	mach_timebase_info_data_t timebase_info;
	mach_timebase_info(&timebase_info);
	const double period_clk = period_ns * (double)timebase_info.denom / (double)timebase_info.numer;

	policy.period = period_clk;
	policy.computation = period_clk * .9;
	policy.constraint = period_clk * .95;
	policy.preemptible = true;
	kern_return_t res = thread_policy_set (pthread_mach_thread_np (thread_id),
			THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t) &policy,
			THREAD_TIME_CONSTRAINT_POLICY_COUNT);

#ifndef NDEBUG
	printf ("Mach Thread(%p) %d %d %d %d OK: %d\n", thread_id, policy.period, policy.computation, policy.constraint, policy.preemptible, res == KERN_SUCCESS);
#endif
	return res != KERN_SUCCESS;
#endif
	return false; // OK
}
