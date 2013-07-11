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

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>
#include <cstdio>

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

#include "pbd/compose.h"
#include "pbd/file_manager.h"
#include "pbd/debug.h"

using namespace std;
using namespace PBD;

FileManager* FileDescriptor::_manager;

FileManager::FileManager ()
	: _open (0)
{
	struct rlimit rl;
	int const r = getrlimit (RLIMIT_NOFILE, &rl);
	
	/* XXX: this is a bit arbitrary */
	if (r == 0) {
		_max_open = rl.rlim_cur - 64;
	} else {
		_max_open = 256;
	}

	DEBUG_TRACE (DEBUG::FileManager, string_compose ("FileManager can open up to %1 files.\n", _max_open));
}

void
FileManager::add (FileDescriptor* d)
{
	Glib::Threads::Mutex::Lock lm (_mutex);
	_files.push_back (d);
}

/** @return true on error, otherwise false */
bool
FileManager::allocate (FileDescriptor* d)
{
	Glib::Threads::Mutex::Lock lm (_mutex);

	if (!d->is_open()) {
		
		/* this file needs to be opened */
		
		if (_open == _max_open) {

			/* We already have the maximum allowed number of files opened, so we must try to close one.
			   Find the unallocated, open file with the lowest last_used time.
			*/

			double lowest_last_used = DBL_MAX;
			list<FileDescriptor*>::iterator oldest = _files.end ();

			for (list<FileDescriptor*>::iterator i = _files.begin(); i != _files.end(); ++i) {
				if ((*i)->is_open() && (*i)->_refcount == 0) {
					if ((*i)->_last_used < lowest_last_used) {
						lowest_last_used = (*i)->_last_used;
						oldest = i;
					}
				}
			}

			if (oldest == _files.end()) {
				/* no unallocated and open files exist, so there's nothing we can do */
				return true;
			}

			close (*oldest);
			DEBUG_TRACE (
				DEBUG::FileManager,
				string_compose (
					"closed file for %1 to release file handle; now have %2 of %3 open\n",
					(*oldest)->_path, _open, _max_open
					)
				);
		}

		if (d->open ()) {
			DEBUG_TRACE (DEBUG::FileManager, string_compose ("open of %1 failed.\n", d->_path));
			return true;
		}

		_open++;

		DEBUG_TRACE (DEBUG::FileManager, string_compose ("opened file for %1; now have %2 of %3 open.\n", d->_path, _open, _max_open));
	}

#ifdef __APPLE__
	d->_last_used = mach_absolute_time();
#elif defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
	struct timespec t;
	clock_gettime (CLOCK_MONOTONIC, &t);
	d->_last_used = t.tv_sec + (double) t.tv_nsec / 10e9;
#else
	struct timeval now;
	gettimeofday (&now, NULL);
	d->_last_used = now.tv_sec + (double) now.tv_usec / 10e6;
#endif

	d->_refcount++;
	
	return false;
}

/** Tell FileManager that a FileDescriptor is no longer needed for a given handle */
void
FileManager::release (FileDescriptor* d)
{
	Glib::Threads::Mutex::Lock lm (_mutex);

	d->_refcount--;
	assert (d->_refcount >= 0);
}

/** Remove a file from our lists.  It will be closed if it is currently open. */
void
FileManager::remove (FileDescriptor* d)
{
	Glib::Threads::Mutex::Lock lm (_mutex);

	if (d->is_open ()) {
		close (d);
		DEBUG_TRACE (
			DEBUG::FileManager,
			string_compose ("closed file for %1; file is being removed; now have %2 of %3 open\n", d->_path, _open, _max_open)
			);
	}

	_files.remove (d);
}

void
FileManager::close (FileDescriptor* d)
{
	/* we must have a lock on our mutex */

	d->close ();
	d->Closed (); /* EMIT SIGNAL */
	_open--;
}

FileDescriptor::FileDescriptor (string const & n, bool w)
	: _refcount (0)
	, _last_used (0)
	, _path (n)
	, _writeable (w)
{

}

FileManager*
FileDescriptor::manager ()
{
	if (_manager == 0) {
		_manager = new FileManager;
	}

	return _manager;
}

/** Release a previously allocated handle to this file */
void
FileDescriptor::release ()
{
	manager()->release (this);
}



/** @param file_name Filename.
 *  @param writeable true to open writeable, otherwise false.
 *  @param mode Open mode for the file.
 */

FdFileDescriptor::FdFileDescriptor (string const & file_name, bool writeable, mode_t mode)
	: FileDescriptor (file_name, writeable)
	, _fd (-1)
	, _mode (mode)
{
	manager()->add (this);
}

FdFileDescriptor::~FdFileDescriptor ()
{
	manager()->remove (this);
}

bool
FdFileDescriptor::is_open () const
{
	/* we must have a lock on the FileManager's mutex */

	return _fd != -1;
}

bool
FdFileDescriptor::open ()
{
	/* we must have a lock on the FileManager's mutex */
	
	_fd = ::open (_path.c_str(), _writeable ? (O_RDWR | O_CREAT) : O_RDONLY, _mode);
	return (_fd == -1);
}

void
FdFileDescriptor::close ()
{
	/* we must have a lock on the FileManager's mutex */

	::close (_fd);
	_fd = -1;
}

/** @return fd, or -1 on error */
int
FdFileDescriptor::allocate ()
{
	bool const f = manager()->allocate (this);
	if (f) {
		return -1;
	}

	/* this is ok thread-wise because allocate () has incremented
	   the Descriptor's refcount, so the file will not be closed
	*/
	return _fd;
}


void
FileDescriptor::set_path (const string& p)
{
        _path = p;
}

/** @param file_name Filename.
 *  @param mode Mode to pass to fopen.
 */

StdioFileDescriptor::StdioFileDescriptor (string const & file_name, std::string const & mode)
	: FileDescriptor (file_name, false)
	, _file (0)
	, _mode (mode)
{
	manager()->add (this);
}

StdioFileDescriptor::~StdioFileDescriptor ()
{
	manager()->remove (this);
}

bool
StdioFileDescriptor::is_open () const
{
	/* we must have a lock on the FileManager's mutex */

	return _file != 0;
}

bool
StdioFileDescriptor::open ()
{
	/* we must have a lock on the FileManager's mutex */
	
	_file = fopen (_path.c_str(), _mode.c_str());
	return (_file == 0);
}

void
StdioFileDescriptor::close ()
{
	/* we must have a lock on the FileManager's mutex */

	fclose (_file);
	_file = 0;
}

/** @return FILE*, or 0 on error */
FILE*
StdioFileDescriptor::allocate ()
{
	bool const f = manager()->allocate (this);
	if (f) {
		return 0;
	}

	/* this is ok thread-wise because allocate () has incremented
	   the Descriptor's refcount, so the file will not be closed
	*/
	return _file;
}
