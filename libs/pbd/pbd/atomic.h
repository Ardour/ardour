/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_atomic_h__
#define __libpbd_atomic_h__

#include <atomic>

namespace PBD {

template<typename T>
bool atomic_dec_and_test (std::atomic<T>& aval) { return (aval.fetch_sub (1) - 1) == 0; }

template<typename T>
void atomic_inc (std::atomic<T>& aval) { (void) aval.fetch_add (1); }

}

#endif /* __libpbd_atomic_h__ */
