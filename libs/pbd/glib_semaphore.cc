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

#include "pbd/glib_semaphore.h"

namespace PBD {

GlibSemaphore::GlibSemaphore (gint initial_val)
	:
		m_counter(initial_val)
{ }

void
GlibSemaphore::wait ()
{
	Glib::Threads::Mutex::Lock guard (m_mutex);

	while (m_counter.get() < 1) {
		m_cond.wait(m_mutex);
	}

	// this shouldn't need to be inside the lock
	--m_counter;
}

bool
GlibSemaphore::try_wait ()
{
	if (!m_mutex.trylock())
	{
		return false;
	}
	// lock successful
	while (m_counter.get() < 1) {
		m_cond.wait(m_mutex);
	}

	// the order of these should not matter
	--m_counter;
	m_mutex.unlock();
	return true;
}

void
GlibSemaphore::post ()
{
	// atomic, no locking required
	++m_counter;
	m_cond.signal();
}

} // namespace PBD
