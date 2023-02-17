/*
 * Copyright (C) 2010 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef PBD_ATOMIC_COUNTER_H
#define PBD_ATOMIC_COUNTER_H

#include <atomic>

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
	{
		m_value.store (value);
	}

	gint get() const
	{
		return m_value.load ();
	}

	void set (gint new_value)
	{
		m_value.store (new_value);
	}

	void increment ()
	{
		m_value.fetch_add (1);
	}

	void operator++ ()
	{
		increment ();
	}

	bool decrement_and_test ()
	{
		return PBD::atomic_dec_and_test (m_value);
	}

	bool operator-- ()
	{
		return decrement_and_test ();
	}

	bool compare_and_exchange (gint old_value, gint new_value)
	{
		return m_value.compare_exchange_strong (old_value, new_value);
	}

	/**
	 * convenience method, \see compare_and_exchange
	 */
	bool cas (gint old_value, gint new_value)
	{
		return compare_and_exchange (old_value, new_value);
	}

private:
	mutable std::atomic<int> m_value;
};

} // namespace PBD

#endif // PBD_ATOMIC_COUNTER_H
