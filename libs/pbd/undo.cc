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

#include <string>
#include <sstream>
#include <time.h>

#include "pbd/undo.h"
#include "pbd/xml++.h"

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
        _timestamp = rhs._timestamp;
	clear ();
	actions.insert(actions.end(),rhs.actions.begin(),rhs.actions.end());
}

UndoTransaction::~UndoTransaction ()
{
	drop_references ();
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
UndoTransaction::add_command (Command *const cmd)
{
	/* catch death of command (e.g. caused by death of object to
	   which it refers. command_death() is a normal static function
	   so there is no need to manage this connection.
	 */

	cmd->DropReferences.connect_same_thread (*this, boost::bind (&command_death, this, cmd));
	actions.push_back (cmd);
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

class UndoRedoSignaller {
public:
    UndoRedoSignaller (UndoHistory& uh) 
	    : _history (uh) { 
	    _history.BeginUndoRedo(); 
    }
    ~UndoRedoSignaller() { 
	    _history.EndUndoRedo(); 
    }

private:
    UndoHistory& _history;
};

UndoHistory::UndoHistory ()
{
	_clearing = false;
	_depth = 0;
}

void
UndoHistory::set_depth (uint32_t d)
{
	UndoTransaction* ut;
	uint32_t current_depth = UndoList.size();

	_depth = d;

	if (d > current_depth) {
		/* not even transactions to meet request */
		return;
	}

	if (_depth > 0) {

		uint32_t cnt = current_depth - d;

		while (cnt--) {
			ut = UndoList.front();
			UndoList.pop_front ();
			delete ut;
		}
	}
}

void
UndoHistory::add (UndoTransaction* const ut)
{
	uint32_t current_depth = UndoList.size();

	ut->DropReferences.connect_same_thread (*this, boost::bind (&UndoHistory::remove, this, ut));

	/* if the current undo history is larger than or equal to the currently
	   requested depth, then pop off at least 1 element to make space
	   at the back for new one.
	*/

	if ((_depth > 0) && current_depth && (current_depth >= _depth)) {

		uint32_t cnt = 1 + (current_depth - _depth);

		while (cnt--) {
			UndoTransaction* ut;
			ut = UndoList.front ();
			UndoList.pop_front ();
			delete ut;
		}
	}

	UndoList.push_back (ut);

	/* we are now owners of the transaction and must delete it when finished with it */

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
	if (n == 0) {
		return;
	}

	{
		UndoRedoSignaller exception_safe_signaller (*this);

		while (n--) {
			if (UndoList.size() == 0) {
				return;
			}
			UndoTransaction* ut = UndoList.back ();
			UndoList.pop_back ();
			ut->undo ();
			RedoList.push_back (ut);
		}
	}

	Changed (); /* EMIT SIGNAL */
}

void
UndoHistory::redo (unsigned int n)
{
	if (n == 0) {
		return;
	}

	{
		UndoRedoSignaller exception_safe_signaller (*this);
		
		while (n--) {
			if (RedoList.size() == 0) {
				return;
			}
			UndoTransaction* ut = RedoList.back ();
			RedoList.pop_back ();
			ut->redo ();
			UndoList.push_back (ut);
		}
	}

	Changed (); /* EMIT SIGNAL */
}

void
UndoHistory::clear_redo ()
{
	_clearing = true;
        for (std::list<UndoTransaction*>::iterator i = RedoList.begin(); i != RedoList.end(); ++i) {
                delete *i;
        }
	RedoList.clear ();
	_clearing = false;

	Changed (); /* EMIT SIGNAL */

}

void
UndoHistory::clear_undo ()
{
	_clearing = true;
        for (std::list<UndoTransaction*>::iterator i = UndoList.begin(); i != UndoList.end(); ++i) {
                delete *i;
        }
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


