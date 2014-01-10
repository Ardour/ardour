/*
    Copyright (C) 2000 Paul Davis & Benno Senoner

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

#ifndef ringbuffer_h
#define ringbuffer_h

#include <cstring>
#include <glib.h>

#include "pbd/libpbd_visibility.h"

template<class T>
class /*LIBPBD_API*/ RingBuffer 
{
  public:
	RingBuffer (guint sz) {
//	size = ffs(sz); /* find first [bit] set is a single inlined assembly instruction. But it looks like the API rounds up so... */
	guint power_of_two;
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
		g_atomic_int_set (&write_idx, 0);
		g_atomic_int_set (&read_idx, 0);
	}

	void set (guint r, guint w) {
		/* !!! NOT THREAD SAFE !!! */
		g_atomic_int_set (&write_idx, w);
		g_atomic_int_set (&read_idx, r);
	}
	
	guint read  (T *dest, guint cnt);
	guint write (T const * src,  guint cnt);

	struct rw_vector {
	    T *buf[2];
	    guint len[2];
	};

	void get_read_vector (rw_vector *);
	void get_write_vector (rw_vector *);
	
	void decrement_read_idx (guint cnt) {
		g_atomic_int_set (&read_idx, (g_atomic_int_get(&read_idx) - cnt) & size_mask);
	}                

	void increment_read_idx (guint cnt) {
		g_atomic_int_set (&read_idx, (g_atomic_int_get(&read_idx) + cnt) & size_mask);
	}                

	void increment_write_idx (guint cnt) {
		g_atomic_int_set (&write_idx,  (g_atomic_int_get(&write_idx) + cnt) & size_mask);
	}                

	guint write_space () {
		guint w, r;
		
		w = g_atomic_int_get (&write_idx);
		r = g_atomic_int_get (&read_idx);
		
		if (w > r) {
			return ((r - w + size) & size_mask) - 1;
		} else if (w < r) {
			return (r - w) - 1;
		} else {
			return size - 1;
		}
	}
	
	guint read_space () {
		guint w, r;
		
		w = g_atomic_int_get (&write_idx);
		r = g_atomic_int_get (&read_idx);
		
		if (w > r) {
			return w - r;
		} else {
			return (w - r + size) & size_mask;
		}
	}

	T *buffer () { return buf; }
	guint get_write_idx () const { return g_atomic_int_get (&write_idx); }
	guint get_read_idx () const { return g_atomic_int_get (&read_idx); }
	guint bufsize () const { return size; }

  protected:
	T *buf;
	guint size;
	mutable gint write_idx;
	mutable gint read_idx;
	guint size_mask;
};

template<class T> /*LIBPBD_API*/ guint 
RingBuffer<T>::read (T *dest, guint cnt)
{
        guint free_cnt;
        guint cnt2;
        guint to_read;
        guint n1, n2;
        guint priv_read_idx;

        priv_read_idx=g_atomic_int_get(&read_idx);

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

        g_atomic_int_set(&read_idx, priv_read_idx);
        return to_read;
}

template<class T> /*LIBPBD_API*/ guint
RingBuffer<T>::write (T const *src, guint cnt)

{
        guint free_cnt;
        guint cnt2;
        guint to_write;
        guint n1, n2;
        guint priv_write_idx;

        priv_write_idx=g_atomic_int_get(&write_idx);

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

        g_atomic_int_set(&write_idx, priv_write_idx);
        return to_write;
}

template<class T> /*LIBPBD_API*/ void
RingBuffer<T>::get_read_vector (typename RingBuffer<T>::rw_vector *vec)

{
	guint free_cnt;
	guint cnt2;
	guint w, r;
	
	w = g_atomic_int_get (&write_idx);
	r = g_atomic_int_get (&read_idx);
	
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
	guint free_cnt;
	guint cnt2;
	guint w, r;
	
	w = g_atomic_int_get (&write_idx);
	r = g_atomic_int_get (&read_idx);
	
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


#endif /* __ringbuffer_h__ */
