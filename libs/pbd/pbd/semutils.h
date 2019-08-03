/*
 * Copyright (C) 2010-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __pbd_semutils_h__
#define __pbd_semutils_h__

#if (defined PLATFORM_WINDOWS && !defined USE_PTW32_SEMAPHORE)
#define WINDOWS_SEMAPHORE 1
#endif

#ifdef WINDOWS_SEMAPHORE
#include <windows.h>
#else
#include <pthread.h>
#include <semaphore.h>
#endif

#include "pbd/libpbd_visibility.h"

namespace PBD {

class LIBPBD_API Semaphore {
  private:
#ifdef WINDOWS_SEMAPHORE
	HANDLE _sem;

#elif __APPLE__
	sem_t* _sem;
	sem_t* ptr_to_sem() const { return _sem; }
#else
	mutable sem_t _sem;
	sem_t* ptr_to_sem() const { return &_sem; }
#endif

  public:
	Semaphore (const char* name, int val);
	~Semaphore ();

#ifdef WINDOWS_SEMAPHORE

	int signal ();
	int wait ();
	int reset ();

#else
	int signal () { return sem_post (ptr_to_sem()); }
	int wait () { return sem_wait (ptr_to_sem()); }
	int reset () { int rv = 0 ; while (sem_trywait (ptr_to_sem()) == 0) ++rv; return rv; }
#endif
};

}

#endif /* __pbd_semutils_h__ */
