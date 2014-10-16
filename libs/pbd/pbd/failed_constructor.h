/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __pbd_failed_constructor_h__
#define __pbd_failed_constructor_h__

#include <exception>

#include "pbd/libpbd_visibility.h"

class LIBPBD_API failed_constructor : public std::exception {
  public:
	virtual const char *what() const throw() { return "failed constructor"; }
};

#endif /* __pbd_failed_constructor_h__ */
