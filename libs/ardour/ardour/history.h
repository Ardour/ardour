/*
    Copyright (C) 2001 Paul Davis 

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

#ifndef __ardour_history_h__
#define __ardour_history_h__

#include <list>
#include <sigc++/signal.h>

template<class T>
//struct History : public SigC::Object 
struct History : public sigc::trackable
{
    typedef list<T *> StateList;
    
    StateList states;
    StateList::iterator current;

    History() { current = states.end(); }
    ~History() { states.erase (states.begin(), states.end()); }

    sigc::signal<void> CurrentChanged;

    void clear () {
	    for (StateList::iterator i = states.begin(); i != states.end(); i++) {
		    delete *i;
	    }
	    states.clear ();
	    current = states.end();
	    CurrentChanged();
    }

    void push (T *state) {
	    /* remove any "undone" history above the current location
	       in the history, before pushing new state. 
	    */
	    if (current != states.begin() && current != states.end()) {
		    states.erase (states.begin(), current);
	    }
	    current = states.insert (states.begin(), state);
	    CurrentChanged ();
    }

    T *top() { 
	    if (current != states.end()) {
		    return *current;
	    } else {
		    return 0;
	    }
    }

    T *pop (bool remove) {
	if (current == states.end()) {
		return 0;
	}
	
	if (current == states.begin()) {
		return *current;
	}
	
	current--;
	T *state = *current;

	if (remove) {
		states.erase (current);
	}

	CurrentChanged ();
	return state;
    }	

    T *earlier (uint32_t n) {
	    StateList::iterator i;
	    
	    if (current == states.end()) {
		    return 0;
	    }
	    
	    if (n == 0) {
		    return *current;
	    }
	    
	    /* the list is in LIFO order, so move toward the end to go "earlier" */
	    
	    for (i = current; n && i != states.end(); i++, n--);
	    
	    if (i == states.end()) {
		    return 0;
	    } else {
		    current = i;
		    CurrentChanged ();
		    return *i;
	    }
    }

    T *later (uint32_t n) {
	    StateList::iterator i;
	    
	    if (current == states.end()) {
		    return 0;
	    }
	    
	    if (n == 0) {
		    return *current;
	    }
	    
	    /* the list is in LIFO order, so move toward the beginning to go "later" */
	    
	    for (i = current; n && i != states.begin(); i--, n--);
	    if (i != current) {
		    current = i;
		    CurrentChanged();
	    }
	    return *i;
    }

    T *nth (uint32_t n) {
	    StateList::iterator i;
	    
	    for (i = states.begin(); n && i != states.end(); n--, i++);
	    
	    if (i != states.end()) {
		    if (i != current) {
			    current = i;
			    CurrentChanged ();
		    }
		    return *i;
	    } else {
		    return 0;
	    }
    }
    
};

#endif /* __ardour_history_h__ */

