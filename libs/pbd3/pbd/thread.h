/*
    Copyright (C) 1998-99 Paul Barton-Davis 

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

#ifndef __qm_thread_h__
#define __qm_thread_h__

#include <pthread.h>

/* A generic base class for Quasimodo objects requiring their own
   thread to do work.
*/

class QMThread 

{
 public:
	QMThread (const char *name, 
		  void *(start)(void *), void *, 
		  bool realtime = false, int rt_priority = 10);

	virtual ~QMThread();

	int   run ();
	void  poke ();
	void  pause ();
	void  stop ();
	void  *wait ();

	/* This doesn't guarantee anything about the state of
	   the thread, but if you do things the right way, and
	   make sure that the do_work() routine checks 
	   work_no_more() at the right times, and that the
	   thread is awake, then calling this will cause
	   the thread to exit fairly quickly.
	*/

	void  halt() { _must_exit = true ; }

	void  exit (void *status);
	pthread_t thread_id() { return _thread; }

	bool thread_ok () { return _have_thread; }
	bool thread_active() { return _thread_active; }

	bool thread_running () {
		/* XXX not atomic */
		return _running && _thread_active;
	}

	bool thread_waiting () { return _thread_waiting; }

	static void try_to_kill_all_threads() {
		all_threads_must_die = true;
	}

 protected:
	void *main ();

	bool  work_no_more () { return (!_running || _must_exit || all_threads_must_die); }

	bool  myself () { 
		return pthread_equal (_thread, pthread_self());
	}

	void suspend() {
		_running = false;
	}

	void lock (pthread_mutex_t *lock) {
		pthread_mutex_lock (lock);
	}
	
	void unlock (pthread_mutex_t *lock) {
		pthread_mutex_unlock (lock);
	}

	virtual void *do_work () = 0;

  private:
	const char *_name;
	bool _must_exit;
	bool _running;
	bool _thread_active;
	bool _thread_waiting;
	bool _have_thread;

	size_t work_cnt;

	pthread_mutex_t status_lock;
	pthread_cond_t wake_up; /* protected by status_lock */
	pthread_cond_t asleep;  /* protected by status_lock */
	pthread_cond_t running; /* protected by status_lock */
	pthread_cond_t exited;  /* protected by status_lock */
	pthread_t _thread;
	
	void lock () { 
		pthread_mutex_lock (&status_lock);
	}

	void unlock () { 
		pthread_mutex_unlock (&status_lock);
	}

	static bool all_threads_must_die;
	
	static void signal_catcher (int sig);
	void setup_signals ();
};

#endif // __qm_thread_h__
