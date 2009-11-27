/*
    Copyright (C) 2009 Paul Davis 
    From an idea by Carl Hetherington.

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

#include "libpbd-config.h"

#ifdef HAVE_EXECINFO
#include <execinfo.h>
#endif

#include <stdlib.h>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <glibmm/thread.h>
#include <boost/shared_ptr.hpp>

#include "pbd/stacktrace.h"

class Backtrace {
public:
    Backtrace ();
    std::ostream& print (std::ostream& str) const;

private:
    void* trace[200];
    size_t size;
};

std::ostream& operator<< (std::ostream& str, const Backtrace& bt) { return bt.print (str); }


Backtrace::Backtrace() 
{ 
#ifdef HAVE_EXECINFO
	size = ::backtrace (trace, 200);
#endif
}

std::ostream&
Backtrace::print (std::ostream& str) const
{
	char **strings = 0;
	size_t i;

	if (size) {
#ifdef HAVE_EXECINFO
		strings = ::backtrace_symbols (trace, size);
#endif		
		if (strings) {
			for (i = 5; i < 5+18 && i < size; i++) {
				str << strings[i] << std::endl;
			}
			free (strings);
		}
	}

	return str;
}

struct BTPair { 

    Backtrace* ref;
    Backtrace* rel;

    BTPair (Backtrace* bt) : ref (bt), rel (0) {}
    ~BTPair () { }

};

std::ostream& operator<<(std::ostream& str, const BTPair& btp) {
	str << "*********************************************\n";
	if (btp.ref) str << *btp.ref << std::endl;
	str << "Rel:\n";
	if (btp.rel) str << *btp.rel << std::endl;
	return str;
}

struct SPDebug { 
    Backtrace* constructor;
    Backtrace* destructor;
    
    SPDebug (Backtrace* c) : constructor (c), destructor (0) {}
    ~SPDebug () {
	    delete constructor;
	    delete destructor;
    }
};

std::ostream& operator<< (std::ostream& str, const SPDebug& spd)
{
	str << "Constructor :" << std::endl;
	if (spd.constructor) {
		str << *spd.constructor << std::endl;
	}

	return str;
}

typedef std::multimap<void const*,SPDebug*> PointerMap;
typedef std::map<void const*,const char*> IPointerMap;

using namespace std;

PointerMap sptrs;
IPointerMap interesting_pointers;

static Glib::StaticMutex the_lock;

static bool
is_interesting_object (void const* ptr)
{
	if (ptr == 0) {
		return false;
	}
	
	return interesting_pointers.find (ptr) != interesting_pointers.end();
}

/* ------------------------------- */

void
boost_debug_shared_ptr_mark_interesting (void* ptr, const char* type)
{
	Glib::Mutex::Lock guard (the_lock);
 	pair<void*,const char*> newpair (ptr, type);
	interesting_pointers.insert (newpair);
}

void
boost_debug_shared_ptr_operator_equals (void const *sp, void const *obj, int)
{
	if (is_interesting_object (obj)) {
		cerr << "sp @ " << sp << " assigned\n";
	}
}

void
boost_debug_shared_ptr_reset (void const *sp, void const *obj, int)
{
	if (is_interesting_object (obj)) {
		cerr << "sp @ " << sp << " reset\n";
	}
}

void
boost_debug_shared_ptr_destructor (void const *sp, void const *obj, int use_count)
{
	Glib::Mutex::Lock guard (the_lock);
	PointerMap::iterator x = sptrs.find (sp);

	if (x != sptrs.end()) {
		sptrs.erase (x);
	}
}
void
boost_debug_shared_ptr_constructor (void const *sp, void const *obj, int use_count)
{

	if (is_interesting_object (obj)) {
		Glib::Mutex::Lock guard (the_lock);
		pair<void const*, SPDebug*> newpair;

		newpair.first = sp;
		newpair.second = new SPDebug (new Backtrace());

		sptrs.insert (newpair);
	}
}

void
boost_debug_list_ptrs ()
{
	Glib::Mutex::Lock guard (the_lock);
	for (PointerMap::iterator x = sptrs.begin(); x != sptrs.end(); ++x) {
		cerr << "Shared ptr @ " << x->first << " history: "
		     << *x->second
		     << endl;
	}
}

namespace boost {

void sp_scalar_constructor_hook( void * object, std::size_t size, void * pn )
{
}

void sp_scalar_destructor_hook( void * object, std::size_t size, void * pn )
{
}

void sp_counter_ref_hook (void* pn, long use_count)
{
}

void sp_counter_release_hook (void* pn, long use_count) 
{
}

void sp_array_constructor_hook(void * p)
{
}

void sp_array_destructor_hook(void * p)
{
}

void sp_scalar_constructor_hook(void * p)
{
}

void sp_scalar_destructor_hook(void * p)
{
}

}
