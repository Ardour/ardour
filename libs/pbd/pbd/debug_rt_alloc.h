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

#ifndef __pbd_debug_rt_alloc_h__
#define __pbd_debug_rt_alloc_h__

#include "pbd/libpbd_visibility.h"

extern "C" {

/** Should be set to point to a function which returns non-0 if a malloc is
 *  allowed in the current situation, or 0 if not.
 */
LIBPBD_API extern int (*pbd_alloc_allowed) ();

/** Call this to suspend malloc checking until a call to resume_rt_malloc_checks */
LIBPBD_API extern void suspend_rt_malloc_checks ();

/** Resume malloc checking after a suspension */	
LIBPBD_API extern void resume_rt_malloc_checks ();

}

#endif

#else

#define suspend_rt_malloc_checks() {}
#define resume_rt_malloc_checks() {}

#endif


