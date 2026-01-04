/*
 * Copyright (C) 2002 The gtkmm Development Team
 * Copyright (C) 2026 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <glib.h>

#include "pbd/libpbd_visibility.h"

namespace PBD
{

template <class T>
class LIBPBD_API Private
{
public:
	typedef void (*DestructorFunc) (void*);

	/** Deletes static_cast<T*>(data) */
	static void delete_ptr (void* data)
	{
		delete static_cast<T*> (data);
	}

	/** Constructor.
	 *
	 * @param destructor_func Function pointer, or 0. If @a destructor_func is not 0
	 * and the stored data pointer is not 0, this function is called when replace()
	 * is called and when the thread exits.
	 */
	explicit inline Private (DestructorFunc destructor_func = &Private<T>::delete_ptr)
	{
		const GPrivate temp = G_PRIVATE_INIT (destructor_func);
		_gobjext            = temp;
	}

	/** Gets the pointer stored in the calling thread.
	 *
	 * @return If no value has yet been set in this thread, 0 is returned.
	 */
	inline T* get ()
	{
		return static_cast<T*> (g_private_get (&_gobjext));
	}

	/** Sets the pointer in the calling thread without calling <tt>destructor_func()</tt>.
	 */
	inline void set (T* data)
	{
		g_private_set (&_gobjext, data);
	}

	/** Sets the pointer in the calling thread and calls <tt>destructor_func()</tt>.
	 * If a function pointer (and not 0) was specified in the constructor, and
	 * the stored data pointer before the call to replace() is not 0, then
	 * <tt>destructor_func()</tt> is called with this old pointer value.
	 *
	 * @newin{2,32}
	 */
	inline void replace (T* data)
	{
		g_private_replace (&_gobjext, data);
	}

private:
	Private (Private<T> const&)               = delete;
	Private<T>& operator= (Private<T> const&) = delete;

	GPrivate _gobjext;
};

} // namespace PBD
