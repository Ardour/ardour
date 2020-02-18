/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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
#ifndef _tlsf_h_
#define _tlsf_h_

#include <string>

#ifndef LIBPBD_API
#include "pbd/libpbd_visibility.h"
#endif

namespace PBD {

class LIBPBD_API TLSF
{
public:
	TLSF (std::string name, size_t bytes);
	~TLSF ();

	void set_name (const std::string& n) { _name = n; }

	static void * lalloc (void* pool, void* ptr, size_t /* oldsize*/, size_t newsize) {
		return static_cast<TLSF*>(pool)->_realloc (ptr, newsize);
	}

	void* malloc (size_t size) { return _malloc (size); }

	void* realloc (void *ptr, size_t newsize) { return _realloc (ptr, newsize); }

	void free (void* ptr) { _free (ptr); }

	size_t get_used_size () const;
	size_t get_max_size () const;

private:
	std::string _name;
	char*_mp;

	void* _malloc (size_t);
	void* _realloc (void *, size_t);
	void  _free (void *);
};

} /* namespace */
#endif // _reallocpool_h_
