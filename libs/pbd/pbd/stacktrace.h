/*
    Copyright (C) 2000-2007 Paul Davis 

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

*/

#ifndef __libpbd_stacktrace_h__
#define __libpbd_stacktrace_h__

#ifdef HAVE_WAFBUILD
#include "libpbd-config.h"
#endif

#include <iostream>
#include <ostream>
#include <glibmm/threads.h>
#include <list>

#ifdef HAVE_EXECINFO
#include <execinfo.h>
#include <cstdlib>
#endif

namespace PBD {
	void stacktrace (std::ostream& out, int levels = 0);
	void trace_twb();

template<typename T>
class thing_with_backtrace 
{
  public:
    thing_with_backtrace () {
	    trace_twb();
#ifdef HAVE_EXECINFO
	    allocation_backtrace = new void*[50];
	    allocation_backtrace_size = backtrace (allocation_backtrace, 50);
#else 
	    allocation_backtrace_size = 0;
#endif
	    Glib::Threads::Mutex::Lock lm (all_mutex);
	    all.push_back (this);
    }

    thing_with_backtrace (const thing_with_backtrace<T>& other) {
	    trace_twb();
#ifdef HAVE_EXECINFO
	    allocation_backtrace = new void*[50];
	    allocation_backtrace_size = backtrace (allocation_backtrace, 50);
#else 
	    allocation_backtrace_size = 0;
#endif
	    Glib::Threads::Mutex::Lock lm (all_mutex);
	    all.push_back (this);
    }

    ~thing_with_backtrace() { 
	    if (allocation_backtrace_size) {
		    delete [] allocation_backtrace;
	    }
	    Glib::Threads::Mutex::Lock lm (all_mutex);
	    all.remove (this);
    }

    thing_with_backtrace<T>& operator= (const thing_with_backtrace<T>& other) {
	    /* no copyable members */
	    return *this;
    }

    static void peek_a_boo (std::ostream& stream) {
#ifdef HAVE_EXECINFO
	    typename std::list<thing_with_backtrace<T>*>::iterator x;
	    for (x = all.begin(); x != all.end(); ++x) {
		    char **strings;
		    size_t i;
		    
		    strings = backtrace_symbols ((*x)->allocation_backtrace, (*x)->allocation_backtrace_size);
		    
		    if (strings) {
			    stream << "--- ALLOCATED SHARED_PTR @ " << (*x) << std::endl;
			    for (i = 0; i < (*x)->allocation_backtrace_size && i < 50U; i++) {
				    stream << strings[i] << std::endl;
			    }
			    free (strings);
		    }
	    }
#else
	    stream << "execinfo not defined for this platform" << std::endl;
#endif
    }

private:
    void** allocation_backtrace;
    int allocation_backtrace_size;
    static std::list<thing_with_backtrace<T>* > all;
    static Glib::Threads::Mutex all_mutex;
};

template<typename T> std::list<PBD::thing_with_backtrace<T> *> PBD::thing_with_backtrace<T>::all;
template<typename T> Glib::Threads::Mutex PBD::thing_with_backtrace<T>::all_mutex;

} // namespace PBD



#endif /* __libpbd_stacktrace_h__ */
