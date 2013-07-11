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

/** @file libs/pbd/sndfile_manager.cc
 *  @brief A FileDescriptor for files opened using libsndfile.
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>
#include "pbd/compose.h"
#include "pbd/sndfile_manager.h"
#include "pbd/debug.h"

using namespace std;
using namespace PBD;

/** @param file_name Filename.
 *  @param writeable true to open writeable, otherwise false.
 *  @param info SF_INFO for the file.
 */

SndFileDescriptor::SndFileDescriptor (string const & file_name, bool writeable, SF_INFO* info)
	: FileDescriptor (file_name, writeable)
	, _sndfile (0)
	, _info (info)
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

	assert (_sndfile);
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
	
	_sndfile = sf_open (_path.c_str(), _writeable ? SFM_RDWR : SFM_READ, _info);
	return (_sndfile == 0);
}

