/*
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef _pbd_spinlock_h_
#define _pbd_spinlock_h_

#include <boost/smart_ptr/detail/spinlock.hpp>
#include <cstring>

#include "pbd/libpbd_visibility.h"

namespace PBD {

/* struct boost::detail::spinlock {
 *   void lock();
 *   bool try_lock();
 *   void unlock();
 * };
 *
 * initialize with BOOST_DETAIL_SPINLOCK_INIT
 */

struct spinlock_t {
public:
#ifdef BOOST_SMART_PTR_DETAIL_SPINLOCK_STD_ATOMIC_HPP_INCLUDED
	/* C++11 non-static data member initialization,
	 * with non-copyable std::atomic ATOMIC_FLAG_INIT
	 */
	spinlock_t () {}
#else
	/* default C++ assign struct's first member */
	spinlock_t ()
	{
		boost::detail::spinlock init = BOOST_DETAIL_SPINLOCK_INIT;
		std::memcpy (&l, &init, sizeof (init));
	}
#endif
	void lock () { l.lock (); }
	void unlock () { l.unlock (); }
	bool try_lock () { return l.try_lock (); }

private:
#ifdef BOOST_SMART_PTR_DETAIL_SPINLOCK_STD_ATOMIC_HPP_INCLUDED
	boost::detail::spinlock l = BOOST_DETAIL_SPINLOCK_INIT;
#else
	boost::detail::spinlock l;
#endif

	/* prevent copy construction */
	spinlock_t (const spinlock_t&);
};

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
