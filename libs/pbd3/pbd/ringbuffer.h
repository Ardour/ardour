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

    $Id$
*/

#ifndef ringbuffer_h
#define ringbuffer_h

#include <sys/mman.h>
#include <pbd/atomic.h>

template<class T>
class RingBuffer 
{
  public:
	RingBuffer (size_t sz) {
		size_t power_of_two;
		
		for (power_of_two = 1; 1U<<power_of_two < sz; power_of_two++);
		
		size = 1<<power_of_two;
		size_mask = size;
		size_mask -= 1;
		buf = new T[size];
		reset ();

	};
	
	virtual ~RingBuffer() {
		delete [] buf;
	}

	void reset () {
		/* !!! NOT THREAD SAFE !!! */
		atomic_set (&write_ptr, 0);
		atomic_set (&read_ptr, 0);
	}

	void set (size_t r, size_t w) {
		/* !!! NOT THREAD SAFE !!! */
		atomic_set (&write_ptr, w);
		atomic_set (&read_ptr, r);
	}
	
	size_t  read  (T *dest, size_t cnt);
	size_t  write (T *src, size_t cnt);

	struct rw_vector {
	    T *buf[2];
	    size_t len[2];
	};

	void get_read_vector (rw_vector *);
	void get_write_vector (rw_vector *);
	
	void decrement_read_ptr (size_t cnt) {
		atomic_set (&read_ptr, (atomic_read(&read_ptr) - cnt) & size_mask);
	}                

	void increment_read_ptr (size_t cnt) {
		atomic_set (&read_ptr, (atomic_read(&read_ptr) + cnt) & size_mask);
	}                

	void increment_write_ptr (size_t cnt) {
		atomic_set (&write_ptr,  (atomic_read(&write_ptr) + cnt) & size_mask);
	}                

	size_t write_space () {
		size_t w, r;
		
		w = atomic_read (&write_ptr);
		r = atomic_read (&read_ptr);
		
		if (w > r) {
			return ((r - w + size) & size_mask) - 1;
		} else if (w < r) {
			return (r - w) - 1;
		} else {
			return size - 1;
		}
	}
	
	size_t read_space () {
		size_t w, r;
		
		w = atomic_read (&write_ptr);
		r = atomic_read (&read_ptr);
		
		if (w > r) {
			return w - r;
		} else {
			return (w - r + size) & size_mask;
		}
	}

	T *buffer () { return buf; }
	size_t get_write_ptr () const { return atomic_read (&write_ptr); }
	size_t get_read_ptr () const { return atomic_read (&read_ptr); }
	size_t bufsize () const { return size; }

  protected:
	T *buf;
	size_t size;
	atomic_t write_ptr;
	atomic_t read_ptr;
	size_t size_mask;
};

template<class T> size_t
RingBuffer<T>::read (T *dest, size_t cnt)
{
        size_t free_cnt;
        size_t cnt2;
        size_t to_read;
        size_t n1, n2;
        size_t priv_read_ptr;

        priv_read_ptr=atomic_read(&read_ptr);

        if ((free_cnt = read_space ()) == 0) {
                return 0;
        }

        to_read = cnt > free_cnt ? free_cnt : cnt;
        
        cnt2 = priv_read_ptr + to_read;

        if (cnt2 > size) {
                n1 = size - priv_read_ptr;
                n2 = cnt2 & size_mask;
        } else {
                n1 = to_read;
                n2 = 0;
        }
        
        memcpy (dest, &buf[priv_read_ptr], n1 * sizeof (T));
        priv_read_ptr = (priv_read_ptr + n1) & size_mask;

        if (n2) {
                memcpy (dest+n1, buf, n2 * sizeof (T));
                priv_read_ptr = n2;
        }

        atomic_set(&read_ptr, priv_read_ptr);
        return to_read;
}

template<class T> size_t
RingBuffer<T>::write (T *src, size_t cnt)

{
        size_t free_cnt;
        size_t cnt2;
        size_t to_write;
        size_t n1, n2;
        size_t priv_write_ptr;

        priv_write_ptr=atomic_read(&write_ptr);

        if ((free_cnt = write_space ()) == 0) {
                return 0;
        }

        to_write = cnt > free_cnt ? free_cnt : cnt;
        
        cnt2 = priv_write_ptr + to_write;

        if (cnt2 > size) {
                n1 = size - priv_write_ptr;
                n2 = cnt2 & size_mask;
        } else {
                n1 = to_write;
                n2 = 0;
        }

        memcpy (&buf[priv_write_ptr], src, n1 * sizeof (T));
        priv_write_ptr = (priv_write_ptr + n1) & size_mask;

        if (n2) {
                memcpy (buf, src+n1, n2 * sizeof (T));
                priv_write_ptr = n2;
        }

        atomic_set(&write_ptr, priv_write_ptr);
        return to_write;
}

template<class T> void
RingBuffer<T>::get_read_vector (RingBuffer<T>::rw_vector *vec)

{
	size_t free_cnt;
	size_t cnt2;
	size_t w, r;
	
	w = atomic_read (&write_ptr);
	r = atomic_read (&read_ptr);
	
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
		vec->len[1] = 0;
	}
}

template<class T> void
RingBuffer<T>::get_write_vector (RingBuffer<T>::rw_vector *vec)

{
	size_t free_cnt;
	size_t cnt2;
	size_t w, r;
	
	w = atomic_read (&write_ptr);
	r = atomic_read (&read_ptr);
	
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
