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
#include "i18n.h"

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

StatefulDiffCommand::StatefulDiffCommand (Stateful* s, XMLNode const & n)
	: _object (s)
{
	_before = new XMLNode (*n.children().front());
	_after = new XMLNode (*n.children().back());
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
	XMLNode* node = new XMLNode (X_("StatefulDiffCommand"));

	node->add_property ("obj-id", _object->id().to_s());
	node->add_property ("type-name", typeid(*_object).name());
	node->add_child_copy (*_before);
	node->add_child_copy (*_after);

	return *node;
}
