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

#include <map>
#include <iostream>
#include <string>
#include <stdint.h>

#include <pbd/pthread_utils.h>
#ifdef WINE_THREAD_SUPPORT
#include <fst.h>
#endif

using namespace std;

typedef std::map<string,pthread_t> ThreadMap;
static ThreadMap all_threads;
static pthread_mutex_t thread_map_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gui_notify_lock = PTHREAD_MUTEX_INITIALIZER;

namespace PBD {
   sigc::signal<void,pthread_t>             ThreadLeaving;
   sigc::signal<void,pthread_t,std::string,uint32_t> ThreadCreatedWithRequestSize;
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
PBD::notify_gui_about_thread_creation (pthread_t thread, std::string str, int request_count)
{
	pthread_mutex_lock (&gui_notify_lock);
	ThreadCreatedWithRequestSize (thread, str, request_count);
	pthread_mutex_unlock (&gui_notify_lock);
}

void
PBD::notify_gui_about_thread_exit (pthread_t thread)
{
	pthread_mutex_lock (&gui_notify_lock);
	ThreadLeaving (thread);
	pthread_mutex_unlock (&gui_notify_lock);
}

int  
pthread_create_and_store (string name, pthread_t  *thread, pthread_attr_t *attr, void * (*start_routine)(void *), void * arg)
{
	int ret;

	pthread_attr_t default_attr;
	bool use_default_attr = (attr == NULL);
	
	if (use_default_attr) {
		// set default stack size to sensible default for memlocking
		pthread_attr_init(&default_attr);
		pthread_attr_setstacksize(&default_attr, 500000);
		attr = &default_attr;
	}

	if ((ret = thread_creator (thread, attr, start_routine, arg)) == 0) {
		std::pair<string,pthread_t> newpair;
		newpair.first = name;
		newpair.second = *thread;

		pthread_mutex_lock (&thread_map_lock);
		all_threads.insert (newpair);

		pthread_mutex_unlock (&thread_map_lock);
	}

	if (use_default_attr) {
		pthread_attr_destroy(&default_attr);
	}
	
	return ret;
}

string
pthread_name ()
{
	pthread_t self = pthread_self();
	string str;

	pthread_mutex_lock (&thread_map_lock);
	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ++i) {
		if (i->second == self) {
			str = i->first;
			pthread_mutex_unlock (&thread_map_lock);
			return str;
		}
	}
	pthread_mutex_unlock (&thread_map_lock);
	return "unknown";
}

void
pthread_kill_all (int signum) 
{	
	pthread_mutex_lock (&thread_map_lock);
	for (ThreadMap::iterator i = all_threads.begin(); i != all_threads.end(); ++i) {
		if (i->second != pthread_self()) {
			pthread_kill (i->second, signum);
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
		if (i->second != pthread_self()) {
			pthread_cancel (i->second);
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
		if (i->second == thread) {
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
		if (i->second == thread) {
			all_threads.erase (i);
			break;
		}
	}
	pthread_mutex_unlock (&thread_map_lock);
	pthread_exit (status);
}
