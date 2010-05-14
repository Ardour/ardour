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
#include <iostream>
#include "pbd/compose.h"
#include "ardour/file_manager.h"
#include "ardour/debug.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

FileManager* FileDescriptor::_manager;

namespace ARDOUR {

/** Class to limit the number of files held open */
class FileManager
{
public:
	FileManager ();
	
	void add (FileDescriptor *);
	void remove (FileDescriptor *);

	void release (FileDescriptor *);
	bool allocate (FileDescriptor *);

private:
	
	void close (FileDescriptor *);

	std::list<FileDescriptor*> _files; ///< files we know about
	Glib::Mutex _mutex; ///< mutex for _files, _open and FileDescriptor contents
	int _open; ///< number of open files
	int _max_open; ///< maximum number of open files
};

}

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
	Glib::Mutex::Lock lm (_mutex);
	_files.push_back (d);
}

/** @return true on error, otherwise false */
bool
FileManager::allocate (FileDescriptor* d)
{
	Glib::Mutex::Lock lm (_mutex);

	if (!d->is_open()) {
		
		/* this file needs to be opened */
		
		if (_open == _max_open) {

			/* We already have the maximum allowed number of files opened, so we must try to close one.
			   Find the unallocated, open file with the lowest last_used time.
			*/

			double lowest_last_used = DBL_MAX;
			list<FileDescriptor*>::iterator oldest = _files.end ();

			for (list<FileDescriptor*>::iterator i = _files.begin(); i != _files.end(); ++i) {
				if ((*i)->is_open() && (*i)->refcount == 0) {
					if ((*i)->last_used < lowest_last_used) {
						lowest_last_used = (*i)->last_used;
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
					(*oldest)->name, _open, _max_open
					)
				);
		}

		if (d->open ()) {
			return true;
		}

		_open++;

		DEBUG_TRACE (DEBUG::FileManager, string_compose ("opened file for %1; now have %2 of %3 open.\n", d->name, _open, _max_open));
	}

	struct timespec t;
	clock_gettime (CLOCK_MONOTONIC, &t);
	d->last_used = t.tv_sec + (double) t.tv_nsec / 10e9;

	d->refcount++;
	
	return false;
}

/** Tell FileManager that a FileDescriptor is no longer needed for a given handle */
void
FileManager::release (FileDescriptor* d)
{
	Glib::Mutex::Lock lm (_mutex);

	d->refcount--;
	assert (d->refcount >= 0);
}

/** Remove a file from our lists.  It will be closed if it is currently open. */
void
FileManager::remove (FileDescriptor* d)
{
	Glib::Mutex::Lock lm (_mutex);

	if (d->is_open ()) {
		close (d);
		DEBUG_TRACE (
			DEBUG::FileManager,
			string_compose ("closed file for %1; file is being removed; now have %2 of %3 open\n", d->name, _open, _max_open)
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
	: refcount (0)
	, last_used (0)
	, name (n)
	, writeable (w)
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

/** @param n Filename.
 *  @param w true to open writeable, otherwise false.
 *  @param i SF_INFO for the file.
 */

SndFileDescriptor::SndFileDescriptor (string const & n, bool w, SF_INFO* i)
	: FileDescriptor (n, w)
	, _sndfile (0)
	, _info (i)
{
	manager()->add (this);
}

SndFileDescriptor::~SndFileDescriptor ()
{
	manager()->remove (this);
}

/** @return SNDFILE*, or 0 on error */
SNDFILE*
SndFileDescriptor::allocate ()
{
	bool const f = manager()->allocate (this);
	if (f) {
		return 0;
	}

	/* this is ok thread-wise because allocate () has incremented
	   the Descriptor's refcount, so the file will not be closed
	*/
	return _sndfile;
}

void
SndFileDescriptor::close ()
{
	/* we must have a lock on the FileManager's mutex */

	sf_close (_sndfile);
	_sndfile = 0;
}

bool
SndFileDescriptor::is_open () const
{
	/* we must have a lock on the FileManager's mutex */

	return _sndfile != 0;
}

bool
SndFileDescriptor::open ()
{
	/* we must have a lock on the FileManager's mutex */
	
	_sndfile = sf_open (name.c_str(), writeable ? SFM_RDWR : SFM_READ, _info);
	return (_sndfile == 0);
}


/** @param n Filename.
 *  @param w true to open writeable, otherwise false.
 *  @param m Open mode for the file.
 */

FdFileDescriptor::FdFileDescriptor (string const & n, bool w, mode_t m)
	: FileDescriptor (n, w)
	, _fd (-1)
	, _mode (m)
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
	
	_fd = ::open (name.c_str(), writeable ? (O_RDWR | O_CREAT) : O_RDONLY, _mode);
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
