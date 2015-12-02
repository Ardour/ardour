/*
    Copyright (C) 2010 Paul Davis

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

#else
	int signal () { return sem_post (ptr_to_sem()); }
	int wait () { return sem_wait (ptr_to_sem()); }
#endif
};

}

#endif /* __pbd_semutils_h__ */
