/*
    Copyright (C) 2002 Paul Davis 

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

#include <set>
#include <string>
#include <cstring>
#include <stdint.h>

#include "pbd/pthread_utils.h"
#ifdef WINE_THREAD_SUPPORT
#include <fst.h>
#endif

using namespace std;

typedef std::set<pthread_t> ThreadMap;
static ThreadMap all_threads;
static pthread_mutex_t thread_map_lock = PTHREAD_MUTEX_INITIALIZER;
static Glib::StaticPrivate<char> thread_name;

namespace PBD {
	PBD::Signal4<void,std::string, pthread_t,std::string,uint32_t> ThreadCreatedWithRequestSize;
}

using namespace PBD;

static int thread_creator (pthread_t* thread_id, const pthread_attr_t* attr, void *(*function)(void*), void* arg)
{
#ifdef WINE_THREAD_SUPPORT
       return wine_pthread_create (thread_id, attr, function, arg);
#else
       return pthread_create (thread_id, attr, function, arg);
#endif
}

void
PBD::notify_gui_about_thread_creation (std::string target_gui, pthread_t thread, std::string str, int request_count)
{
	ThreadCreatedWithRequestSize (target_gui, thread, str, request_count);
}

struct ThreadStartWithName {
    void* (*thread_work)(void*);
    void* arg;
    std::string name;
    
    ThreadStartWithName (void* (*f)(void*), void* a, const std::string& s)
	    : thread_work (f), arg (a), name (s) {}
};

static void*
fake_thread_start (void* arg)
{
	ThreadStartWithName* ts = (ThreadStartWithName*) arg;
	void* (*thread_work)(void*) = ts->thread_work;
	void* thread_arg = ts->arg;

	pthread_set_name (ts->name.c_str());

	delete ts;
	/* name will be deleted by the default handler for GStaticPrivate, when the thread exits */

	return thread_work (thread_arg);
}

int  
pthread_create_and_store (string name, pthread_t  *thread, void * (*start_routine)(void *), void * arg)
{
	pthread_attr_t default_attr;
	int ret;

	// set default stack size to sensible default for memlocking
	pthread_attr_init(&default_attr);
	pthread_attr_setstacksize(&default_attr, 500000);

	ThreadStartWithName* ts = new ThreadStartWithName (start_routine, arg, name);

	if ((ret = thread_creator (thread, &default_attr, fake_thread_start, ts)) == 0) {
		pthread_mutex_lock (&thread_map_lock);
		all_threads.insert (*thread);
		pthread_mutex_unlock (&thread_map_lock);
	}

	pthread_attr_destroy(&default_attr);

	return ret;
}

void
pthread_set_name (const char *str)
{
	/* copy string and delete it when exiting */
	
	thread_name.set (strdup (str), free);
}

const char *
pthread_name ()
{
	const char* str = thread_name.get ();

	if (str) {
		return str;
	} 
	return "unknown";
}

void
pthread_kill_all (int signum) 
{	
	pthread_mutex_lock (&thread_map_lock);
	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ++i) {
		if ((*i) != pthread_self()) {
			pthread_kill ((*i), signum);
		}
	}
	all_threads.clear();
	pthread_mutex_unlock (&thread_map_lock);
}

void
pthread_cancel_all () 
{	
	pthread_mutex_lock (&thread_map_lock);
	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ++i) {
		if ((*i) != pthread_self()) {
			pthread_cancel ((*i));
		}
	}
	all_threads.clear();
	pthread_mutex_unlock (&thread_map_lock);
}

void
pthread_cancel_one (pthread_t thread) 
{	
	pthread_mutex_lock (&thread_map_lock);
	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ++i) {
		if ((*i) == thread) {
			all_threads.erase (i);
			break;
		}
	}

	pthread_cancel (thread);
	pthread_mutex_unlock (&thread_map_lock);
}

void
pthread_exit_pbd (void* status) 
{	
	pthread_t thread = pthread_self();

	pthread_mutex_lock (&thread_map_lock);
	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ++i) {
		if ((*i) == thread) {
			all_threads.erase (i);
			break;
		}
	}
	pthread_mutex_unlock (&thread_map_lock);
	pthread_exit (status);
}
