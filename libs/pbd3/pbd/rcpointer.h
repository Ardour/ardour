/*
    Copyright (C) 1998-99 Paul Barton-Davis 

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

#ifndef __qm_rcpointer_h__
#define __qm_rcpointer_h__

template<class T> class RCPointer {
  public:
	T *operator->() { return _ptr; }
	bool operator==(T *p) { return _ptr == p; }
	bool operator!=(T *p) { return _ptr != p; }

	int refcount() { return _ptr->count; }

	RCPointer () { _ptr = 0; }

	RCPointer (T *p) : _ptr (p) { 
		if (_ptr) _ptr->count++;
	}

	RCPointer (const RCPointer& r) : _ptr (r._ptr) {
		if (_ptr) _ptr->count++;
	}

	RCPointer &operator= (const RCPointer &r) {
		if (_ptr == r._ptr) return *this;
		if (_ptr && --_ptr->count == 0) {
			delete _ptr;
		}
		_ptr = r._ptr;
		if (_ptr) _ptr->count++;
		return *this;
	}
	~RCPointer () { 
		if (_ptr && --_ptr->count == 0) {
			delete _ptr;
		}
	}

  private:
	T *_ptr;
};

#endif  // __qm_rcpointer_h__
