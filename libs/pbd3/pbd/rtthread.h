/*
    Copyright (C) 1998-99 Paul Barton-Davis

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
#ifndef __rtthread_h__
#define __rtthread_h__

#include <pthread.h>

extern int pthread_create_realtime (pthread_t *new_thread,
				    void *(*start)(void *), void *arg,
				    int priority = 10);


#endif  // __rtthread_h__
