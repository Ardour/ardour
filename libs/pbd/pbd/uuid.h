/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __pbd_uuid_h__
#define __pbd_uuid_h__

#include <string>
#include <uuid/uuid.h>

#include "pbd/libpbd_visibility.h"

namespace PBD {

class LIBPBD_API UUID {

  public:
	UUID () { uuid_generate (id); }
	UUID (UUID const & other) { uuid_copy (id, other.id); }
	UUID (std::string const & str) { uuid_parse (str.c_str(), id); }
	
	UUID& operator= (std::string const & str);
	std::string to_s () const;
	
	bool operator== (UUID const & other) const { return !uuid_compare (id, other.id); }
	bool operator!= (UUID const & other) const { return uuid_compare (id, other.id); }
	bool operator< (UUID const & other) const { return uuid_compare (id, other.id) < 0; }
	
	operator bool() const { return !uuid_is_null (id); }

  private:
	uuid_t id;

};

} // namespace PBD

#endif // __pbd_uuid_h__
