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

#ifndef __pbd_sndfile_manager_h__
#define __pbd_sndfile_manager_h__

#include <sys/types.h>
#include <string>
#include <map>
#include <sndfile.h>
#include <glibmm/threads.h>

#include "pbd/libpbd_visibility.h"
#include "pbd/signals.h"
#include "pbd/file_manager.h"

namespace PBD {

/** FileDescriptor for a file to be opened using libsndfile */	
class LIBPBD_API SndFileDescriptor : public FileDescriptor
{
public:
	SndFileDescriptor (std::string const & file_name, bool writeable, SF_INFO* info);
	~SndFileDescriptor ();

	SNDFILE* allocate ();

private:	

	friend class FileManager;

	bool open ();
	void close ();
	bool is_open () const;

	SNDFILE* _sndfile; ///< SNDFILE* pointer, or 0 if the file is closed
	SF_INFO* _info; ///< libsndfile's info for this file
};

}

#endif /* __pbd_sndfile_manager_h__ */
