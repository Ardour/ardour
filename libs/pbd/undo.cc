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

#include <pbd/undo.h>
#include <pbd/xml++.h>
#include <string>

using namespace std;
using namespace sigc;

UndoTransaction::UndoTransaction ()
{
}

UndoTransaction::UndoTransaction (const UndoTransaction& rhs)
{
	_name = rhs._name;
	clear ();
	actions.insert(actions.end(),rhs.actions.begin(),rhs.actions.end());
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
	actions.push_back (action);
}

void
UndoTransaction::clear ()
{
	actions.clear ();
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
	cerr << "Undo " << _name << endl;
	for (list<Command*>::reverse_iterator i = actions.rbegin(); i != actions.rend(); ++i) {
		(*i)->undo();
	}
}

void
UndoTransaction::redo ()
{
	cerr << "Redo " << _name << endl;
        (*this)();
}

XMLNode &UndoTransaction::get_state()
{
    XMLNode *node = new XMLNode ("UndoTransaction");

    list<Command*>::iterator it;
    for (it=actions.begin(); it!=actions.end(); it++)
        node->add_child_nocopy((*it)->get_state());

    return *node;
}

void
UndoHistory::add (UndoTransaction ut)
{
	UndoList.push_back (ut);
}

void
UndoHistory::undo (unsigned int n)
{
	while (n--) {
		if (UndoList.size() == 0) {
			return;
		}
		UndoTransaction ut = UndoList.back ();
		UndoList.pop_back ();
		ut.undo ();
		RedoList.push_back (ut);
	}
}

void
UndoHistory::redo (unsigned int n)
{
	while (n--) {
		if (RedoList.size() == 0) {
			return;
		}
		UndoTransaction ut = RedoList.back ();
		RedoList.pop_back ();
		ut.redo ();
		UndoList.push_back (ut);
	}
}

void
UndoHistory::clear_redo ()
{
	RedoList.clear ();
}

void
UndoHistory::clear_undo ()
{
	UndoList.clear ();
}

void
UndoHistory::clear ()
{
	RedoList.clear ();
	UndoList.clear ();
}

XMLNode & UndoHistory::get_state()
{
    XMLNode *node = new XMLNode ("UndoHistory");

    list<UndoTransaction>::iterator it;
    for (it=UndoList.begin(); it != UndoList.end(); it++)
        node->add_child_nocopy(it->get_state());

    return *node;
}
