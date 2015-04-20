/*
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __libbackend_portaudio_rthread_h__
#define __libbackend_portaudio_rthread_h__

#include <pthread.h>
#include <sched.h>

static int
_realtime_pthread_create (
		const int policy, int priority, const size_t stacksize,
		pthread_t *thread,
		void *(*start_routine) (void *),
		void *arg)
{
	int rv;

	pthread_attr_t attr;
	struct sched_param parm;

	const int p_min = sched_get_priority_min (policy);
	const int p_max = sched_get_priority_max (policy);
	priority += p_max;
	if (priority > p_max) priority = p_max;
	if (priority < p_min) priority = p_min;
	parm.sched_priority = priority;

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

#endif
