/*
 * Copyright (C) 2010 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_property_list_impl_h__
#define __libpbd_property_list_impl_h__

#include "pbd/property_list.h"
#include "pbd/properties.h"

/* now we can define this ... */

namespace PBD {

template<typename T, typename V> bool
PropertyList::add (PropertyDescriptor<T> pid, const V& v) {
        return insert (value_type (pid.property_id, new Property<T> (pid, (T)v))).second;
}

}

#endif /* __libpbd_property_list_impl_h__ */
