/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _pbd_spinlock_h_
#define _pbd_spinlock_h_

#include <boost/smart_ptr/detail/spinlock.hpp>
#include "pbd/libpbd_visibility.h"

namespace PBD {

/* struct boost::detail::spinlock {
 *   void lock();
 *   bool try_lock();
 *   void unlock();
 * };
 */
typedef boost::detail::spinlock spinlock_t;

/* RAII wrapper */
class LIBPBD_API SpinLock {

public:
	SpinLock (spinlock_t&);
	~SpinLock ();

private:
	spinlock_t& _lock;
};

} /* namespace */

#endif /* _pbd_spinlock_h__ */
