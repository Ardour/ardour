/*
    Copyright (C) 2000-2007 Paul Davis

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

*/

#ifndef __pbd_pthread_utils__
#define __pbd_pthread_utils__

/* Accommodate thread setting (and testing) for both
 * 'libpthread' and 'libpthread_win32' (whose implementations
 * of 'pthread_t' are subtlely different)
 */
#ifndef PTHREAD_MACROS_DEFINED
#define PTHREAD_MACROS_DEFINED
#ifdef  PTW32_VERSION  /* pthread_win32 */
#define mark_pthread_inactive(threadID)  threadID.p=0
#define is_pthread_active(threadID)      threadID.p!=0
#else                 /* normal pthread */
#define mark_pthread_inactive(threadID)  threadID=0
#define is_pthread_active(threadID)      threadID!=0
#endif  /* PTW32_VERSION */
#endif  /* PTHREAD_MACROS_DEFINED */

#ifdef COMPILER_MSVC
#include <ardourext/pthread.h>
#else
#include <pthread.h>
#endif
#include <signal.h>
#include <string>
#include <stdint.h>

#include "pbd/libpbd_visibility.h"
#include "pbd/signals.h"

LIBPBD_API int  pthread_create_and_store (std::string name, pthread_t  *thread, void * (*start_routine)(void *), void * arg);
LIBPBD_API void pthread_cancel_one (pthread_t thread);
LIBPBD_API void pthread_cancel_all ();
LIBPBD_API void pthread_kill_all (int signum);
LIBPBD_API const char* pthread_name ();
LIBPBD_API void pthread_set_name (const char* name);

LIBPBD_API int pbd_realtime_pthread_create (
		const int policy, int priority, const size_t stacksize,
		pthread_t *thread,
		void *(*start_routine) (void *),
		void *arg);

LIBPBD_API int  pbd_absolute_rt_priority (int policy, int priority);
LIBPBD_API int  pbd_set_thread_priority (pthread_t, const int policy, int priority);
LIBPBD_API bool pbd_mach_set_realtime_policy (pthread_t thread_id, double period_ns);

namespace PBD {
	LIBPBD_API extern void notify_event_loops_about_thread_creation (pthread_t, const std::string&, int requests = 256);
	LIBPBD_API extern PBD::Signal3<void,pthread_t,std::string,uint32_t> ThreadCreatedWithRequestSize;
}

/* pthread-w32 does not support realtime scheduling
 * (well, windows, doesn't..) and only supports SetThreadPriority()
 *
 * pthread_setschedparam() returns ENOTSUP if the policy is not SCHED_OTHER.
 *
 * however, pthread_create() with attributes, ignores the policy and
 * only sets the priority (when PTHREAD_EXPLICIT_SCHED is used).
 */
#ifdef PLATFORM_WINDOWS
#define PBD_SCHED_FIFO SCHED_OTHER
#else
#define PBD_SCHED_FIFO SCHED_FIFO
#endif
#endif /* __pbd_pthread_utils__ */
