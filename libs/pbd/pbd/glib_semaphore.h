/*
    Copyright (C) 2010 Tim Mayberry

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

#ifndef PBD_GLIB_SEMAPHORE_H
#define PBD_GLIB_SEMAPHORE_H

#include <glibmm/threads.h>

#include "pbd/libpbd_visibility.h"
#include "atomic_counter.h"

namespace PBD {

class LIBPBD_API GlibSemaphore
{

	// prevent copying and assignment
	GlibSemaphore(const GlibSemaphore& sema);
	GlibSemaphore& operator= (const GlibSemaphore& sema);

public:

	GlibSemaphore (gint initial_val = 1);

	void wait ();

	bool try_wait ();

	void post ();

private:

	atomic_counter           m_counter;
	Glib::Threads::Cond      m_cond;
	Glib::Threads::Mutex     m_mutex;

};

} // namespace PBD

#endif // PBD_SEMAPHORE_H
