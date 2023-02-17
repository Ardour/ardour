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

#include <atomic>
#include <cstdint>
#include <cstring>

#include <glibmm.h>

#include "pbd/libpbd_visibility.h"
#include "pbd/spinlock.h"

namespace PBD {

template<class T>
class /*LIBPBD_API*/ PlaybackBuffer
{
public:
	static size_t power_of_two_size (size_t sz) {
		int32_t power_of_two;
		for (power_of_two = 1; 1U << power_of_two < sz; ++power_of_two);
		return 1U << power_of_two;
	}

	PlaybackBuffer (size_t sz, size_t res = 8191)
	: reservation (res)
	{
		sz += reservation;
		size = power_of_two_size (sz);
		size_mask = size - 1;
		buf = new T[size];

		read_idx.store (0);
		reset ();
	}

	virtual ~PlaybackBuffer () {
		delete [] buf;
	}

	/* init (mlock) */
	T *buffer () { return buf; }
	/* init (mlock) */
	size_t bufsize () const { return size; }

	/* write-thread */
	void reset () {
		/* writer, when seeking, may block */
		Glib::Threads::Mutex::Lock lm (_reset_lock);
		SpinLock sl (_reservation_lock);
		read_idx.store (0);
		write_idx.store (0);
		reserved.store (0);
	}

	/* called from rt (reader) thread for new buffers */
	void align_to (PlaybackBuffer const& other) {
		Glib::Threads::Mutex::Lock lm (_reset_lock);
		read_idx.store (other.read_idx.load());
		write_idx.store (other.write_idx.load());
		reserved.store (other.reserved.load());
		memset (buf, 0, size * sizeof (T));
	}

	/* write-thread */
	size_t write_space () const {
		size_t w, r;

		w = write_idx.load ();
		r = read_idx.load ();

		size_t rv;

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
	size_t read_space () const {
		size_t w, r;

		w = write_idx.load ();
		r = read_idx.load ();

		if (w > r) {
			return w - r;
		} else {
			return ((w + size) - r) & size_mask;
		}
	}

	/* write thread */
	size_t overwritable_at (size_t r) const {
		size_t w;

		w = write_idx.load ();

		if (w > r) {
			return w - r;
		}
		return (w - r + size) & size_mask;
	}

	/* read-thead */
	size_t read (T *dest, size_t cnt, bool commit = true, size_t offset = 0);

	/* write-thead */
	size_t write (T const * src, size_t cnt);
	/* write-thead */
	size_t write_zero (size_t cnt);
	/* read-thead */
	size_t increment_write_ptr (size_t cnt)
	{
		cnt = std::min (cnt, write_space ());
		write_idx.store ((write_idx.load () + cnt) & size_mask);
		return cnt;
	}

	/* read-thead */
	size_t decrement_read_ptr (size_t cnt)
	{
		SpinLock sl (_reservation_lock);
		size_t r = read_idx.load ();
		size_t res = reserved.load ();

		cnt = std::min (cnt, res);

		r = (r + size - cnt) & size_mask;
		res -= cnt;

		read_idx.store (r);
		reserved.store (res);

		return cnt;
	}

	/* read-thead */
	size_t increment_read_ptr (size_t cnt)
	{
		cnt = std::min (cnt, read_space ());

		SpinLock sl (_reservation_lock);
		read_idx.store ((read_idx.load () + cnt) & size_mask);
		reserved.store (std::min (reservation, reserved.load () + cnt));

		return cnt;
	}

	/* read-thead */
	bool can_seek (int64_t cnt) {
		if (cnt > 0) {
			return read_space() >= (size_t) cnt;
		} else if (cnt < 0) {
			return reserved.load () >= (size_t) -cnt;
		} else {
			return true;
		}
	}

	size_t read_ptr() const { return read_idx.load (); }
	size_t write_ptr() const { return write_idx.load (); }
	size_t reserved_size() const { return reserved.load (); }
	size_t reservation_size() const { return reservation; }

private:
	T *buf;
	const size_t reservation;
	size_t size;
	size_t size_mask;

	mutable std::atomic<size_t> write_idx;
	mutable std::atomic<size_t> read_idx;
	mutable std::atomic<size_t> reserved;

	/* spinlock will be used to update write_idx and reserved in sync */
	spinlock_t _reservation_lock;
	/* reset_lock is used to prevent concurrent reading and reset (seek, transport reversal etc). */
	Glib::Threads::Mutex _reset_lock;
};

template<class T> /*LIBPBD_API*/ size_t
PlaybackBuffer<T>::write (T const *src, size_t cnt)
{
	size_t w = write_idx.load ();
	const size_t free_cnt = write_space ();

	if (free_cnt == 0) {
		return 0;
	}

	const size_t to_write = cnt > free_cnt ? free_cnt : cnt;
	const size_t cnt2 = w + to_write;

	size_t n1, n2;
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

	write_idx.store (w);
	return to_write;
}

template<class T> /*LIBPBD_API*/ size_t
PlaybackBuffer<T>::write_zero (size_t cnt)
{
	size_t w = write_idx.load ();
	const size_t free_cnt = write_space ();

	if (free_cnt == 0) {
		return 0;
	}

	const size_t to_write = cnt > free_cnt ? free_cnt : cnt;
	const size_t cnt2 = w + to_write;

	size_t n1, n2;
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

	write_idx.store (w);
	return to_write;
}

template<class T> /*LIBPBD_API*/ size_t
PlaybackBuffer<T>::read (T *dest, size_t cnt, bool commit, size_t offset)
{
	Glib::Threads::Mutex::Lock lm (_reset_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked ()) {
		/* seek, reset in progress */
		return 0;
	}

	size_t r = read_idx.load ();
	const size_t w = write_idx.load ();

	size_t free_cnt = (w > r) ? (w - r) : ((w - r + size) & size_mask);

	if (!commit && offset > 0) {
		if (offset > free_cnt) {
			return 0;
		}
		free_cnt -= offset;
		r = (r + offset) & size_mask;
	}

	const size_t to_read = cnt > free_cnt ? free_cnt : cnt;

	const size_t cnt2 = r + to_read;

	size_t n1, n2;
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
		read_idx.store (r);
		reserved.store (std::min (reservation, reserved.load () + to_read));
	}
	return to_read;
}

} /* end namespace */

#endif /* __ringbuffer_h__ */
