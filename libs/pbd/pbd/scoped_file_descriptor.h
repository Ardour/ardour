/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_scoped_file_descriptor_h__
#define __libpbd_scoped_file_descriptor_h__

namespace PBD {

struct ScopedFileDescriptor {
	ScopedFileDescriptor (int fd) : _fd (fd) {}
	~ScopedFileDescriptor() { if (_fd >= 0) close (_fd); }
	operator int() { return _fd; }
	int _fd;
};

}

#endif /* __libpbd_scoped_file_descriptor_h__ */
