/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_property_basics_impl_h__
#define __libpbd_property_basics_impl_h__

namespace PBD {

template<typename T>
PropertyChange::PropertyChange(PropertyDescriptor<T> p)
{
        insert (p.property_id);
}

template<typename T> PropertyChange
PropertyChange::operator=(PropertyDescriptor<T> p)
{
        clear ();
        insert (p.property_id);
        return *this;
}

template<typename T> bool
PropertyChange::contains (PropertyDescriptor<T> p) const
{
        return find (p.property_id) != end ();
}

template<typename T> void
PropertyChange::add (PropertyDescriptor<T> p)
{
        insert (p.property_id);
}

}

#endif /* __libpbd_property_basics_impl_h__ */
