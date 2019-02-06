/*
 * Copyright (C) 2000 Paul Davis & Benno Senoner
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef playback_buffer_h
#define playback_buffer_h

#include <cstring>
#include <glibmm.h>

#include "pbd/libpbd_visibility.h"
#include "pbd/spinlock.h"

namespace PBD {

template<class T>
class /*LIBPBD_API*/ PlaybackBuffer
{
public:
	PlaybackBuffer (int32_t sz, guint res = 8191)
	: reservation (res)
	, _writepos_lock ()
	{
		sz += reservation;

		int32_t power_of_two;
		for (power_of_two = 1; 1U << power_of_two < sz; ++power_of_two);
		size = 1 << power_of_two;

		size_mask = size - 1;
		buf = new T[size];

		read_idx = 0;
		reset (0);
	}

	virtual ~PlaybackBuffer () {
		delete [] buf;
	}

	/* non-linear write needs to reset() the buffer and set the
	 * position that write() will commence at */
	void reset (int64_t start = 0) {
		/* writer, when seeking, may block */
		Glib::Threads::Mutex::Lock lm (_reset_lock);
		SpinLock sl (_writepos_lock);
		write_pos = start;

		g_atomic_int_set (&write_idx, g_atomic_int_get (&read_idx));
	}

	guint write_space () const {
		guint w, r;

		w = g_atomic_int_get (&write_idx);
		r = g_atomic_int_get (&read_idx);

		guint rv;

		if (w > r) {
			rv = (r - w + size) & size_mask;
		} else if (w < r) {
			rv = (r - w);
		} else {
			rv = size;
		}
		/* it may hapen that the read/invalidation-pointer moves backwards
		 * e.g. after rec-stop, declick fade-out.
		 * At the same time the butler may already have written data.
		 * (it's safe as long as the disk-reader does not move backwards by more
		 * than reservation)
		 * XXX disk-reading de-click should not move the invalidation-pointer
		 */
		if (rv > reservation) {
			return rv - 1 - reservation;
		}
		return 0;
	}

	guint read_space () const {
		guint w, r;

		w = g_atomic_int_get (&write_idx);
		r = g_atomic_int_get (&read_idx);

		if (w > r) {
			return w - r;
		} else {
			return (w - r + size) & size_mask;
		}
	}

	guint read  (T *dest, guint cnt, bool commit = true);
	guint write (T const * src, guint cnt);
	guint write_zero (guint cnt);

	T *buffer () { return buf; }
	guint bufsize () const { return size; }

	guint get_write_idx () const { return g_atomic_int_get (&write_idx); }
	guint get_read_idx () const { return g_atomic_int_get (&read_idx); }

	void read_flush () { g_atomic_int_set (&read_idx, g_atomic_int_get (&write_idx)); }

	void increment_read_ptr (guint cnt) {
		cnt = std::min (cnt, read_space ());
		g_atomic_int_set (&read_idx, (g_atomic_int_get (&read_idx) + cnt) & size_mask);
	}

protected:
	T *buf;
	guint reservation;
	guint size;
	guint size_mask;

	int64_t write_pos; // samplepos_t

	mutable gint write_idx; // corresponds to (write_pos)
	mutable gint read_idx;

private:
	/* spinlock will be used to update write_pos and write_idx in sync */
	mutable spinlock_t   _writepos_lock;
	/* reset_lock is used to prevent concurrent reading and reset (seek, transport reversal etc). */
	Glib::Threads::Mutex _reset_lock;
};

template<class T> /*LIBPBD_API*/ guint
PlaybackBuffer<T>::write (T const *src, guint cnt)
{
	guint w = g_atomic_int_get (&write_idx);
	const guint free_cnt = write_space ();

	if (free_cnt == 0) {
		return 0;
	}

	const guint to_write = cnt > free_cnt ? free_cnt : cnt;
	const guint cnt2 = w + to_write;

	guint n1, n2;
	if (cnt2 > size) {
		n1 = size - w;
		n2 = cnt2 & size_mask;
	} else {
		n1 = to_write;
		n2 = 0;
	}

	memcpy (&buf[w], src, n1 * sizeof (T));
	w = (w + n1) & size_mask;

	if (n2) {
		memcpy (buf, src+n1, n2 * sizeof (T));
		w = n2;
	}

	{
		SpinLock sl (_writepos_lock);
		write_pos += to_write;
		g_atomic_int_set (&write_idx, w);
	}
	return to_write;
}

template<class T> /*LIBPBD_API*/ guint
PlaybackBuffer<T>::write_zero (guint cnt)
{
	guint w = g_atomic_int_get (&write_idx);
	const guint free_cnt = write_space ();

	if (free_cnt == 0) {
		return 0;
	}

	const guint to_write = cnt > free_cnt ? free_cnt : cnt;
	const guint cnt2 = w + to_write;

	guint n1, n2;
	if (cnt2 > size) {
		n1 = size - w;
		n2 = cnt2 & size_mask;
	} else {
		n1 = to_write;
		n2 = 0;
	}

	memset (&buf[w], 0, n1 * sizeof (T));
	w = (w + n1) & size_mask;

	if (n2) {
		memset (buf, 0, n2 * sizeof (T));
		w = n2;
	}

	{
		SpinLock sl (_writepos_lock);
		write_pos += to_write;
		g_atomic_int_set (&write_idx, w);
	}
	return to_write;
}

template<class T> /*LIBPBD_API*/ guint
PlaybackBuffer<T>::read (T *dest, guint cnt, bool commit)
{
	Glib::Threads::Mutex::Lock lm (_reset_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked ()) {
		/* seek, reset in progress */
		return 0;
	}

	guint r = g_atomic_int_get (&read_idx);
	const guint w = g_atomic_int_get (&write_idx);

	const guint free_cnt = (w > r) ? (w - r) : ((w - r + size) & size_mask);
	const guint to_read = cnt > free_cnt ? free_cnt : cnt;

	const guint cnt2 = r + to_read;

	guint n1, n2;
	if (cnt2 > size) {
		n1 = size - r;
		n2 = cnt2 & size_mask;
	} else {
		n1 = to_read;
		n2 = 0;
	}

	memcpy (dest, &buf[r], n1 * sizeof (T));
	r = (r + n1) & size_mask;

	if (n2) {
		memcpy (dest + n1, buf, n2 * sizeof (T));
		r = n2;
	}

	if (commit) {
		/* set read-pointer to position of last read's end */
		g_atomic_int_set (&read_idx, r);
	}
	return cnt;
}

} /* end namespace */

#endif /* __ringbuffer_h__ */
