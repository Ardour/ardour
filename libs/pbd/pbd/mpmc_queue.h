/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#ifndef _pbd_mpc_queue_h_
#define _pbd_mpc_queue_h_

#include <cassert>
#include <glib.h>
#include <stdint.h>

#include "pbd/g_atomic_compat.h"

namespace PBD {

/** Lock free multiple producer, multiple consumer queue
 *
 * inspired by http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
 * Kudos to Dmitry Vyukov
 */
template <typename T>
class /*LIBPBD_API*/ MPMCQueue
{
public:
	MPMCQueue (size_t buffer_size = 8)
		: _buffer (0)
		, _buffer_mask (0)
	{
		reserve (buffer_size);
	}

	~MPMCQueue ()
	{
		delete[] _buffer;
	}

	static size_t
	power_of_two_size (size_t sz)
	{
		int32_t power_of_two;
		for (power_of_two = 1; 1U << power_of_two < sz; ++power_of_two) ;
		return 1U << power_of_two;
	}

	void
	reserve (size_t buffer_size)
	{
		buffer_size = power_of_two_size (buffer_size);
		assert ((buffer_size >= 2) && ((buffer_size & (buffer_size - 1)) == 0));
		if (_buffer_mask >= buffer_size - 1) {
			return;
		}
		delete[] _buffer;
		_buffer      = new cell_t[buffer_size];
		_buffer_mask = buffer_size - 1;
		clear ();
	}

	void
	clear ()
	{
		for (size_t i = 0; i <= _buffer_mask; ++i) {
			g_atomic_int_set (&_buffer[i]._sequence, i);
		}
		g_atomic_int_set (&_enqueue_pos, 0);
		g_atomic_int_set (&_dequeue_pos, 0);
	}

	bool
	push_back (T const& data)
	{
		cell_t* cell;
		gint    pos = g_atomic_int_get (&_enqueue_pos);
		for (;;) {
			cell         = &_buffer[pos & _buffer_mask];
			guint    seq = g_atomic_int_get (&cell->_sequence);
			intptr_t dif = (intptr_t)seq - (intptr_t)pos;
			if (dif == 0) {
				if (g_atomic_int_compare_and_exchange (&_enqueue_pos, pos, pos + 1)) {
					break;
				}
			} else if (dif < 0) {
				assert (0);
				return false;
			} else {
				pos = g_atomic_int_get (&_enqueue_pos);
			}
		}

		cell->_data = data;
		g_atomic_int_set (&cell->_sequence, pos + 1);
		return true;
	}

	bool
	pop_front (T& data)
	{
		cell_t* cell;
		gint    pos = g_atomic_int_get (&_dequeue_pos);
		for (;;) {
			cell         = &_buffer[pos & _buffer_mask];
			guint    seq = g_atomic_int_get (&cell->_sequence);
			intptr_t dif = (intptr_t)seq - (intptr_t) (pos + 1);
			if (dif == 0) {
				if (g_atomic_int_compare_and_exchange (&_dequeue_pos, pos, pos + 1)) {
					break;
				}
			} else if (dif < 0) {
				return false;
			} else {
				pos = g_atomic_int_get (&_dequeue_pos);
			}
		}

		data = cell->_data;
		g_atomic_int_set (&cell->_sequence, pos + _buffer_mask + 1);
		return true;
	}

private:
	struct cell_t {
		GATOMIC_QUAL guint _sequence;
		T                  _data;
	};

	cell_t* _buffer;
	size_t  _buffer_mask;

	GATOMIC_QUAL gint _enqueue_pos;
	GATOMIC_QUAL gint _dequeue_pos;
};

} /* end namespace */

#endif
