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

#ifndef PBD_ATOMIC_COUNTER_H
#define PBD_ATOMIC_COUNTER_H

#include <glib.h>

namespace PBD {

class atomic_counter
{
	/**
	 * Prevent copying and assignment
	 */
	atomic_counter (const atomic_counter&);
	atomic_counter& operator= (const atomic_counter&);

public:

	atomic_counter (gint value = 0)
		:
			m_value(value)
	{ }

	gint get() const
	{
		return g_atomic_int_get (&m_value);
	}

	void set (gint new_value)
	{
		g_atomic_int_set (&m_value, new_value);
	}

	void increment ()
	{
		g_atomic_int_inc (&m_value);
	}

	void operator++ ()
	{
		increment ();
	}

	bool decrement_and_test ()
	{
		return g_atomic_int_dec_and_test (&m_value);
	}
	
	bool operator-- ()
	{
		return decrement_and_test ();
	}

	bool compare_and_exchange (gint old_value, gint new_value)
	{
		return g_atomic_int_compare_and_exchange
			(
			 &m_value,
			 old_value,
			 new_value
			);
	}

	/**
	 * convenience method, \see compare_and_exchange
	 */
	bool cas (gint old_value, gint new_value)
	{
		return compare_and_exchange (old_value, new_value);
	}

private:

	// Has to be mutable when using the apple version of gcc.
	mutable volatile gint             m_value;

};

} // namespace PBD

#endif // PBD_ATOMIC_COUNTER_H
