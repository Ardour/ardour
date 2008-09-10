/* 
    Copyright (C) 2001 Brett Viren & Paul Davis

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

#include <iostream>
#include <string>
#include <sstream>
#include <time.h>

#include <pbd/undo.h>
#include <pbd/xml++.h>
#include <pbd/shiva.h>

#include <sigc++/bind.h>

using namespace std;
using namespace sigc;

UndoTransaction::UndoTransaction ()
	: _clearing(false)
{
	gettimeofday (&_timestamp, 0);
}

UndoTransaction::UndoTransaction (const UndoTransaction& rhs)
	: Command(rhs._name)
	, _clearing(false)
{
	clear ();
	actions.insert(actions.end(),rhs.actions.begin(),rhs.actions.end());
}

UndoTransaction::~UndoTransaction ()
{
	GoingAway ();
	clear ();
}

void 
command_death (UndoTransaction* ut, Command* c)
{
	if (ut->clearing()) {
		return;
	}

	ut->remove_command (c);

	if (ut->empty()) {
		delete ut;
	}
}

UndoTransaction& 
UndoTransaction::operator= (const UndoTransaction& rhs)
{
	if (this == &rhs) return *this;
	_name = rhs._name;
	clear ();
	actions.insert(actions.end(),rhs.actions.begin(),rhs.actions.end());
	return *this;
}

void
UndoTransaction::add_command (Command *const action)
{
	/* catch death */
	new PBD::ProxyShiva<Command,UndoTransaction> (*action, *this, &command_death);
	actions.push_back (action);
}

void
UndoTransaction::remove_command (Command* const action)
{
	actions.remove (action);
}

bool
UndoTransaction::empty () const
{
	return actions.empty();
}

void
UndoTransaction::clear ()
{
	_clearing = true;
	for (list<Command*>::iterator i = actions.begin(); i != actions.end(); ++i) {
		delete *i;
	}
	actions.clear ();
	_clearing = false;
}

void
UndoTransaction::operator() ()
{
	for (list<Command*>::iterator i = actions.begin(); i != actions.end(); ++i) {
		(*(*i))();
	}
}

void
UndoTransaction::undo ()
{
	for (list<Command*>::reverse_iterator i = actions.rbegin(); i != actions.rend(); ++i) {
		(*i)->undo();
	}
}

void
UndoTransaction::redo ()
{
        (*this)();
}

XMLNode &UndoTransaction::get_state()
{
    XMLNode *node = new XMLNode ("UndoTransaction");
    stringstream ss;
    ss << _timestamp.tv_sec;
    node->add_property("tv_sec", ss.str());
    ss.str("");
    ss << _timestamp.tv_usec;
    node->add_property("tv_usec", ss.str());
    node->add_property("name", _name);

    list<Command*>::iterator it;
    for (it=actions.begin(); it!=actions.end(); it++)
        node->add_child_nocopy((*it)->get_state());

    return *node;
}

UndoHistory::UndoHistory ()
{
	_clearing = false;
	_depth = 0;
}

void
UndoHistory::set_depth (int32_t d)
{
	_depth = d;

	while (_depth > 0 && UndoList.size() > (uint32_t) _depth) {
		UndoList.pop_front ();
	}
}

void
UndoHistory::add (UndoTransaction* const ut)
{
	ut->GoingAway.connect (bind (mem_fun (*this, &UndoHistory::remove), ut));

	while (_depth > 0 && UndoList.size() > (uint32_t) _depth) {
		UndoList.pop_front ();
	}

	UndoList.push_back (ut);

	/* we are now owners of the transaction */

	Changed (); /* EMIT SIGNAL */
}

void
UndoHistory::remove (UndoTransaction* const ut)
{
	if (_clearing) {
		return;
	}

	UndoList.remove (ut);
	RedoList.remove (ut);

	Changed (); /* EMIT SIGNAL */
}

/** Undo some transactions.
 * @param n Number of transactions to undo.
 */
void
UndoHistory::undo (unsigned int n)
{
	while (n--) {
		if (UndoList.size() == 0) {
			return;
		}
		UndoTransaction* ut = UndoList.back ();
		UndoList.pop_back ();
		ut->undo ();
		RedoList.push_back (ut);
	}

	Changed (); /* EMIT SIGNAL */
}

void
UndoHistory::redo (unsigned int n)
{
	while (n--) {
		if (RedoList.size() == 0) {
			return;
		}
		UndoTransaction* ut = RedoList.back ();
		RedoList.pop_back ();
		ut->redo ();
		UndoList.push_back (ut);
	}

	Changed (); /* EMIT SIGNAL */
}

void
UndoHistory::clear_redo ()
{
	_clearing = true;
	RedoList.clear ();
	_clearing = false;

	Changed (); /* EMIT SIGNAL */

}

void
UndoHistory::clear_undo ()
{
	_clearing = true;
	UndoList.clear ();
	_clearing = false;

	Changed (); /* EMIT SIGNAL */
}

void
UndoHistory::clear ()
{
	clear_undo ();
	clear_redo ();

	Changed (); /* EMIT SIGNAL */
}

XMLNode& 
UndoHistory::get_state (int32_t depth)
{
    XMLNode *node = new XMLNode ("UndoHistory");

    if (depth == 0) {

	    return (*node);

    } else if (depth < 0) {

	    /* everything */

	    for (list<UndoTransaction*>::iterator it = UndoList.begin(); it != UndoList.end(); ++it) {
		    node->add_child_nocopy((*it)->get_state());
	    }

    } else {

	    /* just the last "depth" transactions */

	    list<UndoTransaction*> in_order;

	    for (list<UndoTransaction*>::reverse_iterator it = UndoList.rbegin(); it != UndoList.rend() && depth; ++it, depth--) {
		    in_order.push_front (*it);
	    }

	    for (list<UndoTransaction*>::iterator it = in_order.begin(); it != in_order.end(); it++) {
		    node->add_child_nocopy((*it)->get_state());
	    }
    }

    return *node;
}


