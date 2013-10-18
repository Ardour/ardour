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
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "pbd/libpbd_visibility.h"

namespace PBD {

class LIBPBD_API UUID {

  public:
    UUID ()
            : boost::uuids::uuid (boost::uuids::random_generator()()) {}
    UUID (std::string const & str)
            : boost::uuids::uuid (boost::uuids::string_generator()(str)) {}

    explicit UUID (boost::uuids::uuid const& u)
            : boost::uuids::uuid(u)
    {}

    operator boost::uuids::uuid() {
            return static_cast<boost::uuids::uuid&>(*this);
    }

    operator boost::uuids::uuid() const {
            return static_cast<boost::uuids::uuid const&>(*this);
    }

    UUID& operator= (std::string const & str);
    std::string to_s () const;

    operator bool() const { return !is_nil(); }
};

} // namespace PBD

#endif // __pbd_uuid_h__
