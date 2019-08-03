/*
 * Copyright (C) 2008 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2011-2014 Paul Davis <paul@linuxaudiosystems.com>
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

#include <boost/uuid/uuid_io.hpp>
#include "pbd/uuid.h"

PBD::UUID&
PBD::UUID::operator= (std::string const & str)
{
        boost::uuids::string_generator gen;
        *((boost::uuids::uuid*) this) = gen (str);
	return *this;
}

std::string
PBD::UUID::to_s () const
{
        return boost::uuids::to_string (*this);
}
