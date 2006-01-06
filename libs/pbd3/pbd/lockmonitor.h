/*
    Copyright (C) 2000 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#ifndef __pbd_lockmonitor_h__
#define __pbd_lockmonitor_h__

#include <pthread.h>
#include <pbd/pthread_spinlock.h>

#undef DEBUG_LOCK_MONITOR

#ifdef DEBUG_LOCK_MONITOR
#include <iostream>
#include <ardour/cycles.h>
#endif

namespace PBD
{
class Lock {	
  public:
	Lock() { pthread_mutex_init (&_mutex, 0); } 
	virtual ~Lock() {}

	virtual int lock () { return pthread_mutex_lock (&_mutex); }
	virtual int unlock() { return pthread_mutex_unlock (&_mutex); }

	pthread_mutex_t *mutex() { return &_mutex; }

  protected:
	pthread_mutex_t _mutex;
};

class NonBlockingLock : public Lock {	
  public:
	NonBlockingLock() {}
	~NonBlockingLock(){}
	
	int lock () { return pthread_mutex_lock (&_mutex); }
	int trylock () { return pthread_mutex_trylock (&_mutex); }
	int unlock() { return pthread_mutex_unlock (&_mutex); }
};

class RWLock {	
  public:
	RWLock() { pthread_rwlock_init (&_mutex, 0); } 
	virtual ~RWLock() { pthread_rwlock_destroy(&_mutex); }

	virtual int write_lock () { return pthread_rwlock_wrlock (&_mutex); }
	virtual int read_lock () { return pthread_rwlock_rdlock (&_mutex); }
	virtual int unlock() { return pthread_rwlock_unlock (&_mutex); }

	pthread_rwlock_t *mutex() { return &_mutex; }

  protected:
	pthread_rwlock_t _mutex;
};

class NonBlockingRWLock : public RWLock {	
  public:
	NonBlockingRWLock() {}
	~NonBlockingRWLock(){}
	
	int write_trylock () { return pthread_rwlock_trywrlock (&_mutex); }
	int read_trylock () { return pthread_rwlock_tryrdlock (&_mutex); }
};


class LockMonitor 
{
  public:
	LockMonitor (Lock& lck, unsigned long l, const char *f) 
		: lock (lck)
#ifdef DEBUG_LOCK_MONITOR
		, line (l), file (f) 
#endif
		{

#ifdef DEBUG_LOCK_MONITOR
			unsigned long long when;
			when = get_cycles();
			cerr << when << " lock " << &lock << " at " << line << " in " << file << endl;
#endif
			lock.lock ();
#ifdef DEBUG_LOCK_MONITOR
			when = get_cycles();
			cerr << '\t' << when 
			     << " locked: " 
			     << &lock << " at " 
			     << line << " in " << file << endl;
#endif
		}
	
	~LockMonitor () {
		lock.unlock ();
#ifdef DEBUG_LOCK_MONITOR
		unsigned long long when;
		when = get_cycles();
		cerr << '\t' << when << ' ' 
		     << " UNLOCKED "
		     << &lock << " at " 
		     << line << " in " << file << endl;
#endif
	}
  private:
	Lock& lock;
#ifdef DEBUG_LOCK_MONITOR
	unsigned long line;
	const char * file;
#endif
};

class TentativeLockMonitor 
{
  public:
	TentativeLockMonitor (NonBlockingLock& lck, unsigned long l, const char *f) 
		: lock (lck)
#ifdef DEBUG_LOCK_MONITOR
		, line (l), file (f) 
#endif
		{

#ifdef DEBUG_LOCK_MONITOR
			unsigned long long when;
			when = get_cycles();
			cerr << when << " tentative lock " << &lock << " at " << line << " in " << file << endl;
#endif
			_locked = (lock.trylock() == 0);

#ifdef DEBUG_LOCK_MONITOR
			when = get_cycles();
			cerr << '\t' << when << ' ' 
			     << _locked 
			     << " lock: " 
			     << &lock << " at " 
			     << line << " in " << file << endl;
#endif
		}

	~TentativeLockMonitor () {
		if (_locked) {
			lock.unlock ();
#ifdef DEBUG_LOCK_MONITOR
			unsigned long long when;
			when = get_cycles();
			cerr << '\t' << when << ' ' 
			     << " UNLOCKED "
			     << &lock << " at " 
			     << line << " in " << file << endl;
#endif
		}
	}
	
	bool locked() { return _locked; }

  private:
	NonBlockingLock& lock;
	bool _locked;
#ifdef DEBUG_LOCK_MONITOR
	unsigned long line;
	const char * file;
#endif
};

class SpinLockMonitor 
{
  public:
	SpinLockMonitor (pthread_mutex_t *lck, unsigned long l, const char *f) 
		: lock (lck)
#ifdef DEBUG_LOCK_MONITOR
		, line (l), file (f) 
#endif
		{

#ifdef DEBUG_LOCK_MONITOR
			unsigned long long when;
			when = get_cycles();
			cerr << when << " spinlock " << lck << " at " << line << " in " << file << endl;
#endif
			pthread_mutex_spinlock (lck);
#ifdef DEBUG_LOCK_MONITOR
			when = get_cycles();
			cerr << '\t' << when
			     << " locked at " 
			     << &lock << " at " 
			     << line << " in " << file << endl;
#endif
		}
	
	~SpinLockMonitor () {
		pthread_mutex_unlock (lock);
	}
  private:
	pthread_mutex_t *lock;
#ifdef DEBUG_LOCK_MONITOR
	unsigned long line;
	const char * file;
#endif
};


class RWLockMonitor 
{
  public:
	RWLockMonitor (RWLock& lck, bool write, unsigned long l, const char *f) 
		: lock (lck)
#ifdef DEBUG_LOCK_MONITOR
		, line (l), file (f) 
#endif
		{

#ifdef DEBUG_LOCK_MONITOR
			unsigned long long when;
			when = get_cycles();
			cerr << when << " lock " << &lock << " at " << line << " in " << file << endl;
#endif
			if (write) {
				lock.write_lock ();
			} else {
				lock.read_lock ();
			}
#ifdef DEBUG_LOCK_MONITOR
			when = get_cycles();
			cerr << '\t' << when 
			     << " locked: " 
			     << &lock << " at " 
			     << line << " in " << file << endl;
#endif
		}
	
	~RWLockMonitor () {
		lock.unlock ();
#ifdef DEBUG_LOCK_MONITOR
		unsigned long long when;
		when = get_cycles();
		cerr << '\t' << when << ' ' 
		     << " UNLOCKED "
		     << &lock << " at " 
		     << line << " in " << file << endl;
#endif
	}
  private:
	RWLock& lock;
#ifdef DEBUG_LOCK_MONITOR
	unsigned long line;
	const char * file;
#endif
};

class TentativeRWLockMonitor 
{
  public:
	TentativeRWLockMonitor (NonBlockingRWLock& lck, bool write, unsigned long l, const char *f) 
		: lock (lck)
#ifdef DEBUG_LOCK_MONITOR
		, line (l), file (f) 
#endif
		{

#ifdef DEBUG_LOCK_MONITOR
			unsigned long long when;
			when = get_cycles();
			cerr << when << " tentative lock " << &lock << " at " << line << " in " << file << endl;
#endif
			if (write) {
				_locked = (lock.write_trylock() == 0);
			} else {
				_locked = (lock.read_trylock() == 0);
			}

#ifdef DEBUG_LOCK_MONITOR
			when = get_cycles();
			cerr << '\t' << when << ' ' 
			     << _locked 
			     << " lock: " 
			     << &lock << " at " 
			     << line << " in " << file << endl;
#endif
		}

	~TentativeRWLockMonitor () {
		if (_locked) {
			lock.unlock ();
#ifdef DEBUG_LOCK_MONITOR
			unsigned long long when;
			when = get_cycles();
			cerr << '\t' << when << ' ' 
			     << " UNLOCKED "
			     << &lock << " at " 
			     << line << " in " << file << endl;
#endif
		}
	}
	
	bool locked() { return _locked; }

  private:
	NonBlockingRWLock& lock;
	bool _locked;
#ifdef DEBUG_LOCK_MONITOR
	unsigned long line;
	const char * file;
#endif
};


} /* namespace */

#endif /* __pbd_lockmonitor_h__*/
