/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_inflater_h__
#define __libpbd_inflater_h__

#include <string>

#include "pbd/file_archive.h"
#include "pbd/libpbd_visibility.h"

namespace PBD {
	class Thread;
}

namespace PBD {

class LIBPBD_API Inflater : public PBD::FileArchive
{
  public:
	Inflater (std::string const & archive_path, std::string const & destdir);
	~Inflater ();

	int start ();
	bool running() const { return thread != 0; }
	int  status() const { return _status; }

  private:
	PBD::Thread* thread;
	int _status;
	std::string archive_path;
	std::string destdir;

	void threaded_inflate ();
};

} /* namespace */

#endif 
