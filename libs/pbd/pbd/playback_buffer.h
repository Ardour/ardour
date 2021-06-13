/*
 * Copyright (C) 2000 Paul Davis & Benno Senoner
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

#ifndef playback_buffer_h
#define playback_buffer_h

#include <cstring>
#include <stdint.h>
#include <glibmm.h>

#include "pbd/libpbd_visibility.h"
#include "pbd/spinlock.h"
#include "pbd/g_atomic_compat.h"

namespace PBD {

template<class T>
class /*LIBPBD_API*/ PlaybackBuffer
{
public:
	static guint power_of_two_size (guint sz) {
		int32_t power_of_two;
		for (power_of_two = 1; 1U << power_of_two < sz; ++power_of_two);
		return 1U << power_of_two;
	}

	PlaybackBuffer (guint sz, guint res = 8191)
	: reservation (res)
	{
		sz += reservation;
		size = power_of_two_size (sz);
		size_mask = size - 1;
		buf = new T[size];

		g_atomic_int_set (&read_idx, 0);
		reset ();
	}

	virtual ~PlaybackBuffer () {
		delete [] buf;
	}

	/* init (mlock) */
	T *buffer () { return buf; }
	/* init (mlock) */
	guint bufsize () const { return size; }

	/* write-thread */
	void reset () {
		/* writer, when seeking, may block */
		Glib::Threads::Mutex::Lock lm (_reset_lock);
		SpinLock sl (_reservation_lock);
		g_atomic_int_set (&read_idx, 0);
		g_atomic_int_set (&write_idx, 0);
		g_atomic_int_set (&reserved, 0);
	}

	/* called from rt (reader) thread for new buffers */
	void align_to (PlaybackBuffer const& other) {
		Glib::Threads::Mutex::Lock lm (_reset_lock);
		g_atomic_int_set (&read_idx, g_atomic_int_get (&other.read_idx));
		g_atomic_int_set (&write_idx, g_atomic_int_get (&other.write_idx));
		g_atomic_int_set (&reserved, g_atomic_int_get (&other.reserved));
		memset (buf, 0, size * sizeof (T));
	}

	/* write-thread */
	guint write_space () const {
		guint w, r;

		w = g_atomic_int_get (&write_idx);
		r = g_atomic_int_get (&read_idx);

		guint rv;

		if (w > r) {
			rv = ((r + size) - w) & size_mask;
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

	/* read-thread */
	guint read_space () const {
		guint w, r;

		w = g_atomic_int_get (&write_idx);
		r = g_atomic_int_get (&read_idx);

		if (w > r) {
			return w - r;
		} else {
			return ((w + size) - r) & size_mask;
		}
	}

	/* write thread */
	guint overwritable_at (guint r) const {
		guint w;

		w = g_atomic_int_get (&write_idx);

		if (w > r) {
			return w - r;
		}
		return (w - r + size) & size_mask;
	}

	/* read-thead */
	guint read (T *dest, guint cnt, bool commit = true, guint offset = 0);

	/* write-thead */
	guint write (T const * src, guint cnt);
	/* write-thead */
	guint write_zero (guint cnt);
	/* read-thead */
	guint increment_write_ptr (guint cnt)
	{
		cnt = std::min (cnt, write_space ());
		g_atomic_int_set (&write_idx, (g_atomic_int_get (&write_idx) + cnt) & size_mask);
		return cnt;
	}

	/* read-thead */
	guint decrement_read_ptr (guint cnt)
	{
		SpinLock sl (_reservation_lock);
		guint r = g_atomic_int_get (&read_idx);
		guint res = g_atomic_int_get (&reserved);

		cnt = std::min (cnt, res);

		r = (r + size - cnt) & size_mask;
		res -= cnt;

		g_atomic_int_set (&read_idx, r);
		g_atomic_int_set (&reserved, res);

		return cnt;
	}

	/* read-thead */
	guint increment_read_ptr (guint cnt)
	{
		cnt = std::min (cnt, read_space ());

		SpinLock sl (_reservation_lock);
		g_atomic_int_set (&read_idx, (g_atomic_int_get (&read_idx) + cnt) & size_mask);
		g_atomic_int_set (&reserved, std::min (reservation, g_atomic_int_get (&reserved) + cnt));

		return cnt;
	}

	/* read-thead */
	bool can_seek (int64_t cnt) {
		if (cnt > 0) {
			return read_space() >= cnt;
		} else if (cnt < 0) {
			return g_atomic_int_get (&reserved) >= -cnt;
		} else {
			return true;
		}
	}

	guint read_ptr() const { return g_atomic_int_get (&read_idx); }
	guint write_ptr() const { return g_atomic_int_get (&write_idx); }
	guint reserved_size() const { return g_atomic_int_get (&reserved); }
	guint reservation_size() const { return reservation; }

private:
	T *buf;
	const guint reservation;
	guint size;
	guint size_mask;

	mutable GATOMIC_QUAL gint write_idx;
	mutable GATOMIC_QUAL gint read_idx;
	mutable GATOMIC_QUAL gint reserved;

	/* spinlock will be used to update write_idx and reserved in sync */
	spinlock_t _reservation_lock;
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

	g_atomic_int_set (&write_idx, w);
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

	g_atomic_int_set (&write_idx, w);
	return to_write;
}

template<class T> /*LIBPBD_API*/ guint
PlaybackBuffer<T>::read (T *dest, guint cnt, bool commit, guint offset)
{
	Glib::Threads::Mutex::Lock lm (_reset_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked ()) {
		/* seek, reset in progress */
		return 0;
	}

	guint r = g_atomic_int_get (&read_idx);
	const guint w = g_atomic_int_get (&write_idx);

	guint free_cnt = (w > r) ? (w - r) : ((w - r + size) & size_mask);

	if (!commit && offset > 0) {
		if (offset > free_cnt) {
			return 0;
		}
		free_cnt -= offset;
		r = (r + offset) & size_mask;
	}

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
		SpinLock sl (_reservation_lock);
		g_atomic_int_set (&read_idx, r);
		g_atomic_int_set (&reserved, std::min (reservation, g_atomic_int_get (&reserved) + to_read));
	}
	return to_read;
}

} /* end namespace */

#endif /* __ringbuffer_h__ */
