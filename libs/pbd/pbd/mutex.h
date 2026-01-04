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

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "pbd/libpbd_visibility.h"

namespace PBD
{

/** Basic wrapper around std::mutex
 *
 * Almost equivalent to `using Mutex = std::mutex`
 */
class LIBPBD_API Mutex
{
public:
	enum LockFlags {
		Acquire,
		NotLock,
		TryLock
	};
	Mutex ();

	class Lock;

	void lock ()
	{
		_mutex.lock ();
	}

	bool trylock ()
	{
		return _mutex.try_lock ();
	}

	void unlock ()
	{
		_mutex.unlock ();
	}

private:
	friend class Cond;
	Mutex (Mutex const&)            = delete;
	Mutex& operator= (Mutex const&) = delete;

	std::mutex _mutex;
};

/** RAII style mutex wrapper similar to std::lock_guard<std::mutex> */
class LIBPBD_API Mutex::Lock
{
public:
	Lock (Mutex&, Mutex::LockFlags m = Acquire);
	~Lock ();

	inline void acquire ()
	{
		_mutex.lock ();
		_locked = true;
	}

	inline bool try_acquire ()
	{
		_locked = _mutex.trylock ();
		return _locked;
	}

	inline void release ()
	{
		_mutex.unlock ();
		_locked = false;
	}

	inline bool locked () const
	{
		return _locked;
	}

private:
	Lock (Mutex::Lock const&)                   = delete;
	Mutex::Lock& operator= (Mutex::Lock const&) = delete;

	Mutex& _mutex;
	bool   _locked;
};

class Cond
{
public:
	Cond ();

	void signal ()
	{
		_cond.notify_one ();
	}

	void broadcast ()
	{
		_cond.notify_all ();
	}

	void wait (Mutex& mutex)
	{
		std::unique_lock m (mutex._mutex, std::adopt_lock);
		_cond.wait (m);
	}

	bool wait_for (Mutex& mutex, std::chrono::milliseconds const& rel_time)
	{
		std::unique_lock m (mutex._mutex, std::adopt_lock);
		return std::cv_status::no_timeout == _cond.wait_for (m, rel_time);
	}

private:
	Cond (Cond const&)            = delete;
	Cond& operator= (Cond const&) = delete;

	std::condition_variable _cond;
};

} // namespace PBD
