/*
    Copyright (C) 2000 Paul Barton-Davis 

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

#ifndef __pbd_lockfree_fifo_h__
#define __pbd_lockfree_fifo_h__

#include <sys/types.h>
#include <cstdlib>

template<class T>
class LockFreeFIFO 
{
public:
    LockFreeFIFO (int sz) {
	    size = sz;
	    push_ptr = 0;
	    pop_ptr = 0;
	    buf = new T[size];
    };

    virtual ~LockFreeFIFO() { 
	    delete [] buf;
    }
		  

    int pop (T& r) { 
	    if (pop_ptr == push_ptr) {
		    return -1;
	    } else {
		    r = buf[pop_ptr];
		    pop_ptr++; 
		    if (pop_ptr >= size) {
			    pop_ptr = 0;
		    }
		    return 0;
	    }
    }

    int top (T& r) { 
	    if (pop_ptr == push_ptr) {
		    return -1;
	    } else {
		    r = buf[pop_ptr]; 
		    return 0;
	    }
    }

    int push (T& t) { 
	    if ((size_t) abs (static_cast<int>(push_ptr - pop_ptr)) < size) {
		    buf[push_ptr] = t;
		    push_ptr++;
		    if (push_ptr >= size) {
			    push_ptr = 0;
		    }
		    return 0;
	    } else {
		    return -1;
	    }
    }

  protected:
    T *buf;
    volatile size_t push_ptr;
    volatile size_t pop_ptr;
    size_t size;
};


#endif /* __pbd_lockfree_fifo_h__ */
