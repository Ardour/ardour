/*
    Copyright (C) 2010 Paul Davis 

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

#include "pbd/stateful_diff_command.h"

using namespace std;
using namespace PBD;

/** Create a new StatefulDiffCommand by examining the changes made to a Stateful
 *  since the last time that clear_history was called on it.
 *  @param s Stateful object.
 */

StatefulDiffCommand::StatefulDiffCommand (Stateful* s)
	: _object (s)
{
	pair<XMLNode *, XMLNode*> const p = s->diff ();
	_before = p.first;
	_after = p.second;
}


StatefulDiffCommand::~StatefulDiffCommand ()
{
	delete _before;
	delete _after;
}

void
StatefulDiffCommand::operator() ()
{
	_object->set_state (*_after, Stateful::current_state_version);
}

void
StatefulDiffCommand::undo ()
{
	_object->set_state (*_before, Stateful::current_state_version);
}

XMLNode&
StatefulDiffCommand::get_state ()
{
	/* XXX */
}
