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
#include <glibmm/threads.h>
#include <boost/shared_ptr.hpp>

#include "pbd/stacktrace.h"
#include "pbd/boost_debug.h"

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
			for (i = 3; i < 5+18 && i < size; i++) {
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

static PointerMap* _sptrs;
PointerMap& sptrs() { 
        if (_sptrs == 0) {
                _sptrs = new PointerMap;
        }
        return *_sptrs;
}

static IPointerMap* _interesting_pointers;
IPointerMap& interesting_pointers() { 
        if (_interesting_pointers == 0) {
                _interesting_pointers = new IPointerMap;
        }
        return *_interesting_pointers;
}

static Glib::Threads::Mutex* _the_lock;
static Glib::Threads::Mutex& the_lock() {
        if (_the_lock == 0) {
                _the_lock = new Glib::Threads::Mutex;
        }
        return *_the_lock;
}


static bool
is_interesting_object (void const* ptr)
{
	if (ptr == 0) {
		return false;
	}
	
	return interesting_pointers().find (ptr) != interesting_pointers().end();
}

/* ------------------------------- */

static bool debug_out = false;

void
boost_debug_shared_ptr_show_live_debugging (bool yn)
{
	debug_out = yn;
}

void
boost_debug_shared_ptr_mark_interesting (void* ptr, const char* type)
{
	Glib::Threads::Mutex::Lock guard (the_lock());
 	pair<void*,const char*> newpair (ptr, type);
	interesting_pointers().insert (newpair);
	if (debug_out) {
		cerr << "Interesting object @ " << ptr << " of type " << type << endl;
	}
}

void
boost_debug_shared_ptr_operator_equals (void const *sp, void const *old_obj, int old_use_count,  void const *obj, int new_use_count)
{
	if (old_obj == 0 && obj == 0) {
		return;
	}

	Glib::Threads::Mutex::Lock guard (the_lock());

	if (is_interesting_object  (old_obj) || is_interesting_object (obj)) {
		if (debug_out) {
			cerr << "ASSIGN SWAPS " << old_obj << " & " << obj << endl;
		}
	}

	if (is_interesting_object (old_obj)) {
		if (debug_out) {
			cerr << "\tlost old sp @ " << sp << " for " << old_obj << " UC = " << old_use_count << " now for " << obj << " UC = " << new_use_count 
			     << " (total sp's = " << sptrs().size() << ')' << endl;			
		}
		PointerMap::iterator x = sptrs().find (sp);
		
		if (x != sptrs().end()) {
			sptrs().erase (x);
			if (debug_out) {
				cerr << "\tRemoved (by assignment) sp for " << old_obj << " @ " << sp << " UC = " << old_use_count << " (total sp's = " << sptrs().size() << ')' << endl;
			}
		}
	}

	if (is_interesting_object (obj)) {

		pair<void const*, SPDebug*> newpair;

		newpair.first = sp;
		newpair.second = new SPDebug (new Backtrace());

		sptrs().insert (newpair);
		
		if (debug_out) {
			cerr << "assignment created sp for " << obj << " @ " << sp << " used to point to " << old_obj << " UC = " << old_use_count 
			     << " UC = " << new_use_count 
			     << " (total sp's = " << sptrs().size() << ')' << endl;			
			cerr << *newpair.second << endl;
		}
	} 
}

void
boost_debug_shared_ptr_reset (void const *sp, void const *old_obj, int old_use_count,  void const *obj, int new_use_count)
{
	if (old_obj == 0 && obj == 0) {
		return;
	}

	Glib::Threads::Mutex::Lock guard (the_lock());

	if (is_interesting_object  (old_obj) || is_interesting_object (obj)) {
		if (debug_out) {
			cerr << "RESET SWAPS " << old_obj << " & " << obj << endl;
		}
	}

	if (is_interesting_object (old_obj)) {
		if (debug_out) {
			cerr << "\tlost old sp @ " << sp << " for " << old_obj << " UC = " << old_use_count << " now for " << obj << " UC = " << new_use_count 
			     << " (total sp's = " << sptrs().size() << ')' << endl;			
		}
		PointerMap::iterator x = sptrs().find (sp);
		
		if (x != sptrs().end()) {
			sptrs().erase (x);
			if (debug_out) {
				cerr << "\tRemoved (by reset) sp for " << old_obj << " @ " << sp << " UC = " << old_use_count << " (total sp's = " << sptrs().size() << ')' << endl;
			}
		}
	}

	if (is_interesting_object (obj)) {

		pair<void const*, SPDebug*> newpair;

		newpair.first = sp;
		newpair.second = new SPDebug (new Backtrace());

		sptrs().insert (newpair);
		
		if (debug_out) {
			cerr << "reset created sp for " << obj << " @ " << sp << " used to point to " << old_obj << " UC = " << old_use_count 
			     << " UC = " << new_use_count 
			     << " (total sp's = " << sptrs().size() << ')' << endl;			
			cerr << *newpair.second << endl;
		}
	} 
}

void
boost_debug_shared_ptr_destructor (void const *sp, void const *obj, int use_count)
{
	Glib::Threads::Mutex::Lock guard (the_lock());
	PointerMap::iterator x = sptrs().find (sp);

	if (x != sptrs().end()) {
		sptrs().erase (x);
		if (debug_out) {
			cerr << "Removed sp for " << obj << " @ " << sp << " UC = " << use_count << " (total sp's = " << sptrs().size() << ')' << endl;
		}
	}
}

void
boost_debug_shared_ptr_constructor (void const *sp, void const *obj, int use_count)
{
	if (is_interesting_object (obj)) {
		Glib::Threads::Mutex::Lock guard (the_lock());
		pair<void const*, SPDebug*> newpair;

		newpair.first = sp;
		newpair.second = new SPDebug (new Backtrace());

		sptrs().insert (newpair);
		if (debug_out) {
			cerr << "Stored constructor for " << obj << " @ " << sp << " UC = " << use_count << " (total sp's = " << sptrs().size() << ')' << endl;
			cerr << *newpair.second << endl;
		}
	}
}

void
boost_debug_count_ptrs ()
{
	Glib::Threads::Mutex::Lock guard (the_lock());
	// cerr << "Tracking " << interesting_pointers().size() << " interesting objects with " << sptrs().size () << " shared ptrs\n";
}

void
boost_debug_list_ptrs ()
{
	Glib::Threads::Mutex::Lock guard (the_lock());

	if (sptrs().empty()) {
		cerr << "There are no dangling shared ptrs\n";
	} else {
		for (PointerMap::iterator x = sptrs().begin(); x != sptrs().end(); ++x) {
			cerr << "Shared ptr @ " << x->first << " history: "
			     << *x->second
			     << endl;
		}
	}
}

namespace boost {

void sp_scalar_constructor_hook( void *, std::size_t, void *)
{
}

void sp_scalar_destructor_hook( void *, std::size_t, void *)
{
}

void sp_counter_ref_hook (void* /*pn*/, long /* use count */)
{
}

void sp_counter_release_hook (void* /*pn*/, long /*use_count*/) 
{
}

void sp_array_constructor_hook(void *)
{
}

void sp_array_destructor_hook(void *)
{
}

void sp_scalar_constructor_hook(void *)
{
}

void sp_scalar_destructor_hook(void *)
{
}

}
