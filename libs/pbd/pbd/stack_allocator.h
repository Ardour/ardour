/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifndef PBD_STACK_ALLOCATOR_H
#define PBD_STACK_ALLOCATOR_H

#include <boost/type_traits/aligned_storage.hpp>
#include <limits>

#include "pbd/libpbd_visibility.h"

#if 0
# include <cstdio>
# define DEBUG_STACK_ALLOC(...) printf (__VA_ARGS__)
#else
# define DEBUG_STACK_ALLOC(...)
#endif

namespace PBD {

template <class T, std::size_t stack_capacity>
class /*LIBPBD_API*/ StackAllocator
{
public:
#if 0 /* may be needed for compatibility */
	typedef typename std::allocator<T>::value_type value_type;
	typedef typename std::allocator<T>::size_type size_type;
	typedef typename std::allocator<T>::difference_type difference_type;
	typedef typename std::allocator<T>::pointer pointer;
	typedef typename std::allocator<T>::const_pointer const_pointer;
	typedef typename std::allocator<T>::reference reference;
	typedef typename std::allocator<T>::const_reference const_reference;
#else
	typedef T                 value_type;
	typedef std::size_t       size_type;
	typedef std::ptrdiff_t    difference_type;
	typedef value_type*       pointer;
	typedef const value_type* const_pointer;
	typedef value_type&       reference;
	typedef const value_type& const_reference;
#endif

	template <class U>
	struct rebind {
		typedef StackAllocator<U, stack_capacity> other;
	};

	StackAllocator ()
		: _ptr ((pointer)&_buf)
	{ }

	StackAllocator (const StackAllocator&)
		: _ptr ((pointer)&_buf)
	{ }

	template <typename U, size_t other_capacity>
	StackAllocator (const StackAllocator<U, other_capacity>&)
		: _ptr ((pointer)&_buf)
	{ }

	/* inspired by http://howardhinnant.github.io/stack_alloc.h */
	pointer allocate (size_type n, void* hint = 0)
	{
		if ((pointer)&_buf + stack_capacity >= _ptr + n) {
			DEBUG_STACK_ALLOC ("Allocate %ld item(s) of size %zu on the stack\n", n, sizeof (T));
			pointer rv = _ptr;
			_ptr += n;
			return rv;
		} else {
			DEBUG_STACK_ALLOC ("Allocate using new (%ld * %zu)\n", n, sizeof (T));
			return static_cast<pointer> (::operator new (n * sizeof (T)));
		}
	}

	void deallocate (pointer p, size_type n)
	{
		if (pointer_in_buffer (p)) {
			if (p + n == _ptr) {
				DEBUG_STACK_ALLOC ("Deallocate: pop item from the top of the stack\n");
				_ptr = p;
			} else {
				DEBUG_STACK_ALLOC ("Deallocate: ignored. Item is not at the top of the stack \n");
			}
		} else {
			::operator delete (p);
		}
	}

	size_type max_size () const throw ()
	{
		return std::numeric_limits<size_type>::max () / sizeof (T);
	}

	bool operator== (StackAllocator const& a) const
	{
		return &_buf == &a._buf;
	}

	bool operator!= (StackAllocator const& a) const
	{
		return &_buf != &a._buf;
	}

	template <class U>
	void destroy (U* const p)
	{
		p->~U ();
	}

	template <class U>
	void construct (U* const p)
	{
		new (p) U ();
	}

#if __cplusplus > 201103L || defined __clang__
	template <class U, class A>
	void construct (U* const p, A* const a)
	{
		new (p) U (a);
	}
#else
	template <class U, class A>
	void construct (U* const p, A const& a)
	{
		new (p) U (a);
	}
#endif

private:
	StackAllocator& operator= (const StackAllocator&);

	bool pointer_in_buffer (pointer const p)
	{
		return ((pointer const)&_buf <= p && p < (pointer const)&_buf + stack_capacity);
	}

	typedef typename boost::aligned_storage<sizeof (T) * stack_capacity, 16>::type align_t;

	align_t _buf;
	pointer _ptr;
};

} // namespace PBD

#endif
