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

using namespace std;
using namespace sigc;

UndoCommand::UndoCommand ()
{
}

UndoCommand::UndoCommand (const UndoCommand& rhs)
{
	_name = rhs._name;
	clear ();
	undo_actions.insert(undo_actions.end(),rhs.undo_actions.begin(),rhs.undo_actions.end());
	redo_actions.insert(redo_actions.end(),rhs.redo_actions.begin(),rhs.redo_actions.end());
}

UndoCommand& 
UndoCommand::operator= (const UndoCommand& rhs)
{
	if (this == &rhs) return *this;
	_name = rhs._name;
	clear ();
	undo_actions.insert(undo_actions.end(),rhs.undo_actions.begin(),rhs.undo_actions.end());
	redo_actions.insert(redo_actions.end(),rhs.redo_actions.begin(),rhs.redo_actions.end());
	return *this;
}

void
UndoCommand::add_undo (const UndoAction& action)
{
	undo_actions.push_back (action);
}

void
UndoCommand::add_redo (const UndoAction& action)
{
	redo_actions.push_back (action);
	redo_actions.back()(); // operator()
}

void
UndoCommand::add_redo_no_execute (const UndoAction& action)
{
	redo_actions.push_back (action);
}

void
UndoCommand::clear ()
{
	undo_actions.clear ();
	redo_actions.clear ();
}

void
UndoCommand::undo ()
{
	for (list<UndoAction>::reverse_iterator i = undo_actions.rbegin(); i != undo_actions.rend(); ++i) {
		(*i)();
	}
}

void
UndoCommand::redo ()
{
	for (list<UndoAction>::iterator i = redo_actions.begin(); i != redo_actions.end(); ++i) {
		(*i)();
	}
}

void
UndoHistory::add (UndoCommand uc)
{
	UndoList.push_back (uc);
}

void
UndoHistory::undo (unsigned int n)
{
	while (n--) {
		if (UndoList.size() == 0) {
			return;
		}
		UndoCommand uc = UndoList.back ();
		UndoList.pop_back ();
		uc.undo ();
		RedoList.push_back (uc);
	}
}

void
UndoHistory::redo (unsigned int n)
{
	while (n--) {
		if (RedoList.size() == 0) {
			return;
		}
		UndoCommand cmd = RedoList.back ();
		RedoList.pop_back ();
		cmd.redo ();
		UndoList.push_back (cmd);
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
