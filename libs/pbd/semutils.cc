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

#include "pbd/semutils.h"
#include "pbd/failed_constructor.h"

using namespace PBD;

ProcessSemaphore::ProcessSemaphore (const char* name, int val)
{
#ifdef WIN32
	if ((_sem = CreateSemaphore(NULL, val, 32767, name)) == NULL) {
		throw failed_constructor ();
	}

#elif __APPLE__
	if ((_sem = sem_open (name, O_CREAT, 0600, val)) == (sem_t*) SEM_FAILED) {
		throw failed_constructor ();
	}

	/* this semaphore does not exist for IPC */
	
	if (sem_unlink (name)) {
		throw failed_constructor ();
	}

#else
	if (sem_init (&_sem, 0, val)) {
		throw failed_constructor ();
	}
#endif
}

ProcessSemaphore::~ProcessSemaphore ()
{
#ifdef WIN32
	CloseHandle(_sem);
#elif __APPLE__
	sem_close (ptr_to_sem());
#endif
}

#ifdef WIN32

int
ProcessSemaphore::signal ()
{
	// non-zero on success, opposite to posix
	return !ReleaseSemaphore(_sem, 1, NULL);
}

int
ProcessSemaphore::wait ()
{
	DWORD result = 0;
	result = WaitForSingleObject(_sem, INFINITE);
	return (result == WAIT_OBJECT_0);
}

#endif
