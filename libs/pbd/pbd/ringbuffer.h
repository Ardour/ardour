/*
 * Copyright (C) 2000-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
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

#ifndef ringbuffer_h
#define ringbuffer_h

#include <atomic>
#include <cstring>

#include "pbd/libpbd_visibility.h"

namespace PBD {

template<class T>
class /*LIBPBD_API*/ RingBuffer
{
public:
	RingBuffer (size_t sz) {
#if 0
		size = ffs(sz); /* find first [bit] set is a single inlined assembly instruction. But it looks like the API rounds up so... */
#endif
		size_t power_of_two;
		for (power_of_two = 1; 1U<<power_of_two < sz; power_of_two++) {}
		size = 1<<power_of_two;
		size_mask = size;
		size_mask -= 1;
		buf = new T[size];
		reset ();
	}

	virtual ~RingBuffer() {
		delete [] buf;
	}

	void reset () {
		/* !!! NOT THREAD SAFE !!! */
		write_idx.store (0);
		read_idx.store (0);
	}

	void set (size_t r, size_t w) {
		/* !!! NOT THREAD SAFE !!! */
		write_idx.store (w);
		read_idx.store (r);
	}

	size_t read  (T *dest, size_t cnt);
	size_t write (T const * src,  size_t cnt);

	struct rw_vector {
		T *buf[2];
		size_t len[2];
	};

	void get_read_vector (rw_vector *);
	void get_write_vector (rw_vector *);

	void decrement_read_idx (size_t cnt) {
		read_idx.store ((read_idx.load() - cnt) & size_mask);
	}

	void increment_read_idx (size_t cnt) {
		read_idx.store ((read_idx.load () + cnt) & size_mask);
	}

	void increment_write_idx (size_t cnt) {
		write_idx.store ((write_idx.load () + cnt) & size_mask);
	}

	size_t write_space () const {
		size_t w, r;

		w = write_idx.load ();
		r = read_idx.load ();

		if (w > r) {
			return ((r - w + size) & size_mask) - 1;
		} else if (w < r) {
			return (r - w) - 1;
		} else {
			return size - 1;
		}
	}

	size_t read_space () const {
		size_t w, r;

		w = write_idx.load ();
		r = read_idx.load ();

		if (w > r) {
			return w - r;
		} else {
			return (w - r + size) & size_mask;
		}
	}

	T *buffer () { return buf; }
	size_t get_write_idx () const { return write_idx.load (); }
	size_t get_read_idx () const { return read_idx.load (); }
	size_t bufsize () const { return size; }

protected:
	T *buf;
	size_t size;
	size_t size_mask;
	mutable std::atomic<int> write_idx;
	mutable std::atomic<int> read_idx;

private:
	RingBuffer (RingBuffer const&);
};

template<class T> /*LIBPBD_API*/ size_t
RingBuffer<T>::read (T *dest, size_t cnt)
{
	size_t free_cnt;
	size_t cnt2;
	size_t to_read;
	size_t n1, n2;
	size_t priv_read_idx;

	priv_read_idx = read_idx.load ();

	if ((free_cnt = read_space ()) == 0) {
		return 0;
	}

	to_read = cnt > free_cnt ? free_cnt : cnt;

	cnt2 = priv_read_idx + to_read;

	if (cnt2 > size) {
		n1 = size - priv_read_idx;
		n2 = cnt2 & size_mask;
	} else {
		n1 = to_read;
		n2 = 0;
	}

	memcpy (dest, &buf[priv_read_idx], n1 * sizeof (T));
	priv_read_idx = (priv_read_idx + n1) & size_mask;

	if (n2) {
		memcpy (dest+n1, buf, n2 * sizeof (T));
		priv_read_idx = n2;
	}

	read_idx.store (priv_read_idx);
	return to_read;
}

template<class T> /*LIBPBD_API*/ size_t
RingBuffer<T>::write (T const *src, size_t cnt)

{
	size_t free_cnt;
	size_t cnt2;
	size_t to_write;
	size_t n1, n2;
	size_t priv_write_idx;

	priv_write_idx = write_idx.load ();

	if ((free_cnt = write_space ()) == 0) {
		return 0;
	}

	to_write = cnt > free_cnt ? free_cnt : cnt;

	cnt2 = priv_write_idx + to_write;

	if (cnt2 > size) {
		n1 = size - priv_write_idx;
		n2 = cnt2 & size_mask;
	} else {
		n1 = to_write;
		n2 = 0;
	}

	memcpy (&buf[priv_write_idx], src, n1 * sizeof (T));
	priv_write_idx = (priv_write_idx + n1) & size_mask;

	if (n2) {
		memcpy (buf, src+n1, n2 * sizeof (T));
		priv_write_idx = n2;
	}

	write_idx.store (priv_write_idx);
	return to_write;
}

template<class T> /*LIBPBD_API*/ void
RingBuffer<T>::get_read_vector (typename RingBuffer<T>::rw_vector *vec)

{
	size_t free_cnt;
	size_t cnt2;
	size_t w, r;

	w = write_idx.load ();
	r = read_idx.load ();

	if (w > r) {
		free_cnt = w - r;
	} else {
		free_cnt = (w - r + size) & size_mask;
	}

	cnt2 = r + free_cnt;

	if (cnt2 > size) {
		/* Two part vector: the rest of the buffer after the
		   current write ptr, plus some from the start of
		   the buffer.
		*/

		vec->buf[0] = &buf[r];
		vec->len[0] = size - r;
		vec->buf[1] = buf;
		vec->len[1] = cnt2 & size_mask;

	} else {

		/* Single part vector: just the rest of the buffer */

		vec->buf[0] = &buf[r];
		vec->len[0] = free_cnt;
		vec->buf[1] = 0;
		vec->len[1] = 0;
	}
}

template<class T> /*LIBPBD_API*/ void
RingBuffer<T>::get_write_vector (typename RingBuffer<T>::rw_vector *vec)

{
	size_t free_cnt;
	size_t cnt2;
	size_t w, r;

	w = write_idx.load ();
	r = read_idx.load ();

	if (w > r) {
		free_cnt = ((r - w + size) & size_mask) - 1;
	} else if (w < r) {
		free_cnt = (r - w) - 1;
	} else {
		free_cnt = size - 1;
	}

	cnt2 = w + free_cnt;

	if (cnt2 > size) {

		/* Two part vector: the rest of the buffer after the
		   current write ptr, plus some from the start of
		   the buffer.
		*/

		vec->buf[0] = &buf[w];
		vec->len[0] = size - w;
		vec->buf[1] = buf;
		vec->len[1] = cnt2 & size_mask;
	} else {
		vec->buf[0] = &buf[w];
		vec->len[0] = free_cnt;
		vec->len[1] = 0;
	}
}

} /* end namespace */

#endif /* __ringbuffer_h__ */
