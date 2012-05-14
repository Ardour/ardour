/*
  Copyright (C) 2012 Paul Davis
  Author: David Robillard

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

#ifndef __pbd_semaphore_h__
#define __pbd_semaphore_h__

#ifdef __APPLE__
#    include <mach/mach.h>
#elif defined(_WIN32)
#    include <windows.h>
#else
#    include <semaphore.h>
#    include <errno.h>
#endif

#include "pbd/failed_constructor.h"

namespace PBD {

/**
   Unnamed (process local) counting semaphore.

   The civilized person's synchronisation primitive.  A counting semaphore is
   an integer which is always non-negative, so, an attempted decrement (or
   "wait") will block if the value is 0, until another thread does an increment
   (or "post").

   At least on Lignux, the main advantage of this is that it is fast and the
   only safe way to reliably signal from a real-time audio thread.  The
   counting semantics also complement ringbuffers of events nicely.
*/
class Semaphore
{
public:
	/**
	   Create a new semaphore.

	   Chances are you want 1 wait() per 1 post(), an initial value of 0.
	*/
	inline Semaphore(unsigned initial);

	inline ~Semaphore();

	/** Post/Increment/Signal */
	inline void post();

	/** Wait/Decrement.  Returns false on error. */
	inline bool wait();

	/** Attempt Wait/Decrement.  Returns true iff a decrement occurred. */
	inline bool try_wait();

private:
#if defined(__APPLE__)
	semaphore_t _sem;  // sem_t is a worthless broken mess on OSX
#elif defined(_WIN32)
	HANDLE _sem;  // types are overrated anyway
#else
	sem_t _sem;
#endif
};

#ifdef __APPLE__

inline
Semaphore::Semaphore(unsigned initial)
{
	if (semaphore_create(mach_task_self(), &_sem, SYNC_POLICY_FIFO, initial)) {
		throw failed_constructor();
	}
}

inline
Semaphore::~Semaphore()
{
	semaphore_destroy(mach_task_self(), _sem);
}

inline void
Semaphore::post()
{
	semaphore_signal(_sem);
}

inline bool
Semaphore::wait()
{
	if (semaphore_wait(_sem) != KERN_SUCCESS) {
		return false;
	}
	return true;
}

inline bool
Semaphore::try_wait()
{
	const mach_timespec_t zero = { 0, 0 };
	return semaphore_timedwait(_sem, zero) == KERN_SUCCESS;
}

#elif defined(_WIN32)

inline
Semaphore::Semaphore(unsigned initial)
{
	if (!(_sem = CreateSemaphore(NULL, initial, LONG_MAX, NULL))) {
		throw failed_constructor();
	}
}

inline
Semaphore::~Semaphore()
{
	CloseHandle(_sem);
}

inline void
Semaphore::post()
{
	ReleaseSemaphore(_sem, 1, NULL);
}

inline bool
Semaphore::wait()
{
	if (WaitForSingleObject(_sem, INFINITE) != WAIT_OBJECT_0) {
		return false;
	}
	return true;
}

inline bool
Semaphore::try_wait()
{
	return WaitForSingleObject(_sem, 0) == WAIT_OBJECT_0;
}

#else  /* !defined(__APPLE__) && !defined(_WIN32) */

Semaphore::Semaphore(unsigned initial)
{
	if (sem_init(&_sem, 0, initial)) {
		throw failed_constructor();
	}
}

inline
Semaphore::~Semaphore()
{
	sem_destroy(&_sem);
}

inline void
Semaphore::post()
{
	sem_post(&_sem);
}

inline bool
Semaphore::wait()
{
	while (sem_wait(&_sem)) {
		if (errno != EINTR) {
			return false;  // We are all doomed
		}
		/* Otherwise, interrupted (rare/weird), so try again. */
	}

	return true;
}

inline bool
Semaphore::try_wait()
{
	return (sem_trywait(&_sem) == 0);
}

#endif

}  // namespace PBD

#endif  /* __pbd_semaphore_h__ */
