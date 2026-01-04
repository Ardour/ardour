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

#include "pbd/mutex.h"
#include <pthread.h>

using namespace PBD;

Mutex::Lock::Lock (Mutex& mutex, Mutex::LockFlags m)
	: _mutex (mutex)
	, _locked (false)
{
	switch (m) {
		case Acquire:
			acquire ();
			break;
		case TryLock:
			try_acquire ();
			break;
		case NotLock:
			break;
	}
}

Mutex::Lock::~Lock ()
{
	if (_locked) {
		_mutex.unlock ();
	}
}

Mutex::Mutex () {}

Cond::Cond () {}
