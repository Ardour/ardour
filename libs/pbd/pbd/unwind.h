/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __libpbd_unwinder_h__
#define __libpbd_unwinder_h__

#include "pbd/libpbd_visibility.h"

namespace PBD {

template <typename T>
class LIBPBD_API Unwinder {
  public:
    Unwinder (T& var, T new_val) : _var (var), _old_val (var) { var = new_val; }
    ~Unwinder () { _var = _old_val; }
		
  private:
    T& _var;
    T  _old_val;
};

}

#endif /* __libpbd_unwinder_h__ */
