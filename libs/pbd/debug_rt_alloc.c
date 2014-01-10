/*
    Copyright (C) 2011 Paul Davis

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

#ifdef DEBUG_RT_ALLOC

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "pbd/pthread_utils.h"

int (*pbd_alloc_allowed) () = 0;

/** Thread-local key whose value is set to 1 if malloc checking is disabled
 *  for this thread, 0 otherwise.
 */

static pthread_key_t disabled;

static pthread_once_t once;

static void
make_key (void)
{
	(void) pthread_key_create (&disabled, NULL);
}

/** This is our malloc which overrides the system one */
void* malloc (size_t s)
{
	static void * (*real_malloc) (size_t) = NULL;
	if (!real_malloc) {
		/* find the system malloc */
		real_malloc = dlsym (RTLD_NEXT, "malloc");
	}

	(void) pthread_once (&once, make_key);

	if (pthread_getspecific (disabled) == NULL && pbd_alloc_allowed && !pbd_alloc_allowed ()) {
		/* pbd_alloc_allowed says that this malloc is not permitted */
		abort ();
	}

	/* Pass through to the system malloc */
	return real_malloc (s);
}

void
suspend_rt_malloc_checks ()
{
	(void) pthread_once (&once, make_key);
	pthread_setspecific (disabled, (void *) 1);
}

void
resume_rt_malloc_checks ()
{
	(void) pthread_once (&once, make_key);
	pthread_setspecific (disabled, (void *) 0);
}

#endif
