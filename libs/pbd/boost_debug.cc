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

#include <stdlib.h>
#include <iostream>
#include <map>
#include <set>
#include <list>
#include <glibmm/thread.h>
#include <boost/shared_ptr.hpp>

#include "libpbd-config.h"

#include "pbd/stacktrace.h"

class Backtrace {
public:
    Backtrace (int addsub, int use_count, void const* pn);
    std::ostream& print (std::ostream& str) const;
    int use_count() const { return _use_count; }
    void const* pn() const { return _pn; }
    int addsub() const { return _addsub; }

private:
    int _addsub;
    void const* _pn;
    void* trace[200];
    size_t size;
    int _use_count;
};

std::ostream& operator<< (std::ostream& str, const Backtrace& bt) { return bt.print (str); }

#ifdef HAVE_EXECINFO

#include <execinfo.h>

Backtrace::Backtrace(int addsub, int uc, void const* pn) { 
	_addsub = addsub;
	_pn = pn;
	_use_count = uc;
	size = ::backtrace (trace, 200);
}

std::ostream&
Backtrace::print (std::ostream& str) const
{
	char **strings;
	size_t i;

	if (size) {
		strings = ::backtrace_symbols (trace, size);
		
		if (strings) {
			for (i = 5; i < 5+12; i++) {
				str << strings[i] << std::endl;
			}
			free (strings);
		}
	}

	return str;
}

#else 

Backtrace::Backtrace(int addsub, int uc, void const* sp) { 
	_addsub = addsub;
	_pn = pn;
	size = 0;
	_use_count = uc;
}

std::ostream&
Backtrace::print (std::ostream& str) const
{
	return str << "No shared ptr debugging available on this platform\n";
}

#endif

typedef std::list<Backtrace*> BacktraceList;
typedef std::multimap<void const*,BacktraceList*> TraceMap;
typedef std::map<void const*,const char*> PointerMap;
typedef std::map<void const*,void const*> PointerSet;

static TraceMap traces;
static PointerMap interesting_pointers;
static PointerSet interesting_counters;
static Glib::StaticMutex the_lock;

using namespace std;

static void
trace_it (int addsub, void const* pn, void const* object, int use_count)
{
	Glib::Mutex::Lock guard (the_lock);
	Backtrace* bt = new Backtrace (addsub, use_count, pn);
	TraceMap::iterator i = traces.find (const_cast<void*>(object));

	if (i == traces.end()) {
		pair<void const *,BacktraceList*> newpair;
		newpair.first = object;
		newpair.second = new BacktraceList;
		newpair.second->push_back (bt);
		traces.insert (newpair);
		cerr << "Start tracelist for " << object << endl;
	} else {
		i->second->push_back (bt);
		cerr << "Extend tracelist for " << object << endl;
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
		BacktraceList* bt;
		bt = i->second;
		for (BacktraceList::iterator x = bt->begin(); x != bt->end(); ++x) {
			const char* op;
			switch ((*x)->addsub()) {
				case 0:
					op = "CONST";
					break;
				case -1:
					op = "REL";
					break;
				case 1:
					op = "REF";
					break;

				case 2:
					op = "DESTROY";
					break;
			}
			str << "\n**********************************************\n"
			    << "Back trace for " << op << " on " << ptr << " w/use_count = " << (*x)->use_count() << endl;
			str << **x;
		}
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
		trace_it (2, pn, object, ((boost::detail::sp_counted_base*)pn)->use_count());
	}

	Glib::Mutex::Lock guard (the_lock);

	range = traces.equal_range (object);
	
	if (range.first != traces.end()) {

		for (TraceMap::iterator i = range.first; i != range.second; ++i) {
			BacktraceList* btlist;

			btlist = i->second;
			for (BacktraceList::iterator x = btlist->begin(); x != btlist->end(); ) {
				if ((*x)->pn() == pn) {
					delete *x;
					x = btlist->erase (x);
				} else {
					++x;
				}
			}

			if (use_count == 1) {
				btlist->clear ();
				delete btlist;
			}
		}

		if (use_count == 1) {
			traces.erase (range.first, range.second);
		}
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
		//cerr << "UC for " << pn << " inc from " << use_count << endl;
		trace_it (1, pn, i->second, use_count);
	}
}
void sp_counter_release_hook (void* pn, long use_count) 
{
	PointerSet::iterator i = interesting_counters.find (pn);
	if (i != interesting_counters.end()) {
		cerr << "UC for " << pn << " dec from " << use_count << endl;
		trace_it (-1, pn, i->second, use_count);
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
