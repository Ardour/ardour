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
    Backtrace (int op, int use_count, void const* pn);
    std::ostream& print (std::ostream& str) const;
    int use_count() const { return _use_count; }
    void const* pn() const { return _pn; }
    int op() const { return _op; }

private:
    int _op;
    void const* _pn;
    void* trace[200];
    size_t size;
    int _use_count;
};

std::ostream& operator<< (std::ostream& str, const Backtrace& bt) { return bt.print (str); }


Backtrace::Backtrace(int op, int uc, void const* pn) { 
	_op = op;
	_pn = pn;
	_use_count = uc;
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
		str << "BT generated with use count = " << _use_count << std::endl;

#ifdef HAVE_EXECINFO
		strings = ::backtrace_symbols (trace, size);
#endif		
		if (strings) {
			for (i = 5; i < 5+12; i++) {
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
    long use_count;
    
    BTPair (Backtrace* bt, long uc) : ref (bt), rel (0), use_count (uc) {}
    ~BTPair () { }

};

std::ostream& operator<<(std::ostream& str, const BTPair& btp) {
	str << "*********************************************\n";
	str << "@ " << btp.use_count << " Ref:\n"; 
	if (btp.ref) str << *btp.ref << std::endl;
	str << "Rel:\n";
	if (btp.rel) str << *btp.rel << std::endl;
	return str;
}

struct SPDebug { 
    Backtrace* constructor;
    Backtrace* destructor;
    std::vector<BTPair> others;

    SPDebug (Backtrace* c) : constructor (c), destructor (0) {}
    ~SPDebug () {
	    delete constructor;
	    delete destructor;
    }
};

std::ostream& operator<< (std::ostream& str, const SPDebug& spd)
{
	str << "Concerructor :" << std::endl;
	if (spd.constructor) {
		str << *spd.constructor << std::endl;
	}
	str << "\nDestructor :" << std::endl;
	if (spd.destructor) {
		str << *spd.destructor << std::endl;
	}
	
	for (std::vector<BTPair>::const_iterator x = spd.others.begin(); x != spd.others.end(); ++x) {
		str << *x << std::endl;
	}

	return str;
}

typedef std::multimap<void const*,SPDebug*> TraceMap;
typedef std::map<void const*,const char*> PointerMap;
typedef std::map<void const*,void const*> PointerSet;

static TraceMap traces;
static PointerMap interesting_pointers;
static PointerSet interesting_counters;
static Glib::StaticMutex the_lock;

using namespace std;

static void
trace_it (int op, void const* pn, void const* object, int use_count)
{
	Glib::Mutex::Lock guard (the_lock);
	Backtrace* bt = new Backtrace (op, use_count, pn);
	TraceMap::iterator i = traces.find (const_cast<void*>(object));

	if (i == traces.end()) {
		pair<void const *,SPDebug*> newpair;
		newpair.first = object;
		
		if (op != 0) {
			cerr << "SPDEBUG: non-constructor op without entry in trace map\n";
			return;
		}
		newpair.second = new SPDebug (bt);
		traces.insert (newpair);

	} else {
		if (op == 0) {
			cerr << "SPDEBUG: claimed constructor, but have range for this object\n";
			return;
		} else if (op == 2) {
			i->second->destructor = bt;
		} else {
			SPDebug* spd = i->second;

			if (spd->others.empty() || op == 1) {
				spd->others.push_back (BTPair (bt, use_count));
			} else {
				if (op == -1) {
					if (spd->others.back().use_count == use_count-1) {
						spd->others.back().rel = bt;
					} else {
						cerr << "********** FLoating REL\n";
						spd->others.push_back (BTPair (0, use_count));
						spd->others.back().rel = bt;
					}
				} else {
					cerr << "SPDEBUG: illegal op number " << op << endl;
					abort ();
				}
			}
		}
	}
}

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
	cerr << "New interesting pointer: " << ptr << " type = " << type << endl;
	for (PointerMap::iterator i = interesting_pointers.begin(); i != interesting_pointers.end(); ++i) {
		cerr << "IP : " << i->first << " type = " << i->second << endl;
	}	
	cerr << "is interesting ? " << is_interesting_object (ptr) << endl;
}

void
boost_debug_shared_ptr_show (ostream& str, void* ptr)
{
	Glib::Mutex::Lock guard (the_lock);
	pair<TraceMap::iterator,TraceMap::iterator> range;

	range = traces.equal_range (ptr);

	if (range.first == traces.end()) {
		str << "No shared_ptr debugging information found for " << ptr << endl;
		return;
	}

	str << "\n\n--------------------------------------------------------\ninfo for " << ptr << endl;

	for (TraceMap::iterator i = range.first; i != range.second; ++i) {
		str << *i->second << endl;
	}
}

namespace boost {

void sp_scalar_constructor_hook( void * object, std::size_t size, void * pn )
{
	if (is_interesting_object (object)) {
		cerr << "Interesting counter @ " << pn << endl;
		pair<void const*,void const*> newpair (pn, object);
		interesting_counters.insert (newpair);
		trace_it (0, pn, object, ((boost::detail::sp_counted_base*)pn)->use_count());
		
	}
}

void sp_scalar_destructor_hook( void * object, std::size_t size, void * pn )
{
	pair<TraceMap::iterator,TraceMap::iterator> range;
	long use_count = ((boost::detail::sp_counted_base*)pn)->use_count();

	if (is_interesting_object (object)) {
		trace_it (-1, pn, object, use_count);
	}

	if (use_count == 1) {
		// PointerMap::iterator p = interesting_pointers.find (object);
		
		//if (p != interesting_pointers.end()) {
			// interesting_pointers.erase (p);
		//}
	}
}

void sp_counter_ref_hook (void* pn, long use_count)
{
	PointerSet::iterator i = interesting_counters.find (pn);
	if (i != interesting_counters.end()) {
		// cerr << "UC for " << pn << " inc from " << use_count << endl;
		trace_it (1, pn, i->second, use_count);
	}
}
void sp_counter_release_hook (void* pn, long use_count) 
{
	PointerSet::iterator i = interesting_counters.find (pn);
	if (i != interesting_counters.end()) {
		// cerr << "UC for " << pn << " dec from " << use_count << endl;
		if (use_count == 1) {
			trace_it (2, pn, i->second, use_count);
		} else {
			trace_it (-1, pn, i->second, use_count);
		}
	}
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
