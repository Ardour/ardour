/*
 * Copyright (C) 2026 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <pthread.h>

#include "pbd/libpbd_visibility.h"

namespace PBD
{

class LIBPBD_API RWLock
{
public:
	enum LockFlags {
		Lock,
		NotLock,
		TryLock
	};

	class ReaderLock;
	class WriterLock;

	RWLock ();
	~RWLock ();

	inline void reader_lock ()
	{
		pthread_rwlock_rdlock (&_rw_lock);
	}
	inline bool reader_trylock ()
	{
		return 0 == pthread_rwlock_tryrdlock (&_rw_lock);
	}
	inline void reader_unlock ()
	{
		pthread_rwlock_unlock (&_rw_lock);
	}

	inline void writer_lock ()
	{
		pthread_rwlock_wrlock (&_rw_lock);
	}
	inline bool writer_trylock ()
	{
		return 0 == pthread_rwlock_trywrlock (&_rw_lock);
	}
	inline void writer_unlock ()
	{
		pthread_rwlock_unlock (&_rw_lock);
	}

private:
	RWLock (const RWLock&)            = delete;
	RWLock& operator= (const RWLock&) = delete;

	pthread_rwlock_t _rw_lock;
};

class LIBPBD_API RWLock::ReaderLock
{
public:
	ReaderLock (RWLock& rwlock, RWLock::LockFlags m = Lock);
	~ReaderLock ();

	inline void acquire ()
	{
		_rwlock.reader_lock ();
		_locked = true;
	}

	inline bool try_acquire ()
	{
		_locked = _rwlock.reader_trylock ();
		return _locked;
	};

	inline void release ()
	{
		_rwlock.reader_unlock ();
		_locked = false;
	}

	inline bool locked () const
	{
		return _locked;
	}

private:
	RWLock& _rwlock;
	bool    _locked;

	ReaderLock (RWLock::ReaderLock const&)                    = delete;
	RWLock::ReaderLock& operator= (RWLock::ReaderLock const&) = delete;
};

class LIBPBD_API RWLock::WriterLock
{
public:
	WriterLock (RWLock& rwlock, RWLock::LockFlags m = Lock);
	~WriterLock ();

	inline void acquire ()
	{
		_rwlock.writer_lock ();
		_locked = true;
	}

	inline bool try_acquire ()
	{
		_locked = _rwlock.writer_trylock ();
		return _locked;
	}

	inline void release ()
	{
		_rwlock.writer_unlock ();
		_locked = false;
	}

	inline bool locked () const
	{
		return _locked;
	}

private:
	RWLock& _rwlock;
	bool    _locked;

	WriterLock (RWLock::WriterLock const&)                    = delete;
	RWLock::WriterLock& operator= (RWLock::WriterLock const&) = delete;
};

} // namespace PBD
