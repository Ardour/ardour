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

#ifndef __pbd_file_manager_h__
#define __pbd_file_manager_h__

#include <sys/types.h>
#include <string>
#include <map>
#include <list>
#include <glibmm/threads.h>
#include "pbd/signals.h"

namespace PBD {

class FileManager;

/** Parent class for FileDescriptors.
 *
 *  When a subclass is instantiated, the file it describes is added to a
 *  list.  The FileDescriptor can be `allocated', meaning that its
 *  file will be opened on the filesystem, and can then be `released'.
 *  FileDescriptors are reference counted as they are allocated and
 *  released.  When a descriptor's refcount is 0, the file on the
 *  filesystem is eligible to be closed if necessary to free up file
 *  handles for other files.
 *
 *  The upshot of all this is that Ardour can manage the number of
 *  open files to stay within limits imposed by the operating system.
 */
	
class FileDescriptor
{
public:
	FileDescriptor (std::string const &, bool);
	virtual ~FileDescriptor () {}

        const std::string& path() const { return _path; }

	void release ();
        virtual void set_path (const std::string&);

	/** Emitted when the file is closed */
	PBD::Signal0<void> Closed;

protected:

	friend class FileManager;

	/* These methods and variables must be called / accessed
	   with a lock held on the FileManager's mutex
	*/

	/** @return false on success, true on failure */
	virtual bool open () = 0;
	virtual void close () = 0;
	virtual bool is_open () const = 0;

	int _refcount; ///< number of active users of this file
	double _last_used; ///< monotonic time that this file was last allocated
	std::string _path; ///< file path
	bool _writeable; ///< true if it should be opened writeable, otherwise false

	FileManager* manager ();
	
private:
	
	static FileManager* _manager;
};


/** FileDescriptor for a file to be opened using POSIX open */	
class FdFileDescriptor : public FileDescriptor
{
public:
	FdFileDescriptor (std::string const & file_name, bool writeable, mode_t mode);
	~FdFileDescriptor ();

	int allocate ();

private:

	friend class FileManager;

	bool open ();
	void close ();
	bool is_open () const;

	int _fd; ///< file descriptor, or -1 if the file is closed
	mode_t _mode; ///< mode to use when creating files
};

/** FileDescriptor for a file opened using stdio */
class StdioFileDescriptor : public FileDescriptor
{
public:
	StdioFileDescriptor (std::string const & file_name, std::string const & mode);
	~StdioFileDescriptor ();

	FILE* allocate ();

private:

	friend class FileManager;

	bool open ();
	void close ();
	bool is_open () const;

	FILE* _file;
	std::string _mode;
};


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
	Glib::Threads::Mutex _mutex; ///< mutex for _files, _open and FileDescriptor contents
	int _open; ///< number of open files
	int _max_open; ///< maximum number of open files
};

}

#endif
