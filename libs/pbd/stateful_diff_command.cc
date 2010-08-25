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

#include <iostream>

#include "pbd/stateful_diff_command.h"
#include "pbd/property_list.h"
#include "pbd/demangle.h"
#include "i18n.h"

using namespace std;
using namespace PBD;

/** Create a new StatefulDiffCommand by examining the changes made to a Stateful
 *  since the last time that clear_history was called on it.
 *  @param s Stateful object.
 */

StatefulDiffCommand::StatefulDiffCommand (boost::shared_ptr<StatefulDestructible> s)
        : _object (s)
        , _undo (new PropertyList)
        , _redo (new PropertyList)
{
        s->diff (*_undo, *_redo, this);

        /* if the stateful object that this command refers to goes away,
           be sure to notify owners of this command.
        */

        s->DropReferences.connect_same_thread (*this, boost::bind (&Destructible::drop_references, this));
}

StatefulDiffCommand::StatefulDiffCommand (boost::shared_ptr<StatefulDestructible> s, XMLNode const & n)
	: _object (s)
        , _undo (0)
        , _redo (0)
{
        const XMLNodeList& children (n.children());

        for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
                if ((*i)->name() == X_("Undo")) {
                        _undo = s->property_factory (**i);
                } else if ((*i)->name() == X_("Do")) {
                        _redo = s->property_factory (**i);
                }
        }

        assert (_undo != 0);
        assert (_redo != 0);

        /* if the stateful object that this command refers to goes away,
           be sure to notify owners of this command.
        */

        s->DropReferences.connect_same_thread (*this, boost::bind (&Destructible::drop_references, this));
}

StatefulDiffCommand::~StatefulDiffCommand ()
{
        drop_references ();

        delete _undo;
        delete _redo;
}

void
StatefulDiffCommand::operator() ()
{
	boost::shared_ptr<Stateful> s (_object.lock());

	if (s) {
                s->apply_changes (*_redo);
	}
}

void
StatefulDiffCommand::undo ()
{
	boost::shared_ptr<Stateful> s (_object.lock());

	if (s) {
                std::cerr << "Undoing a stateful diff command\n";
                s->apply_changes (*_undo);
	}
}

XMLNode&
StatefulDiffCommand::get_state ()
{
	boost::shared_ptr<Stateful> s (_object.lock());

	if (!s) {
		/* XXX should we throw? */
		return * new XMLNode("");
	}

	XMLNode* node = new XMLNode (X_("StatefulDiffCommand"));

	node->add_property ("obj-id", s->id().to_s());
	node->add_property ("type-name", demangled_name (*s.get()));

        XMLNode* undo = new XMLNode (X_("Undo"));
        XMLNode* redo = new XMLNode (X_("Do"));

        _undo->get_changes (undo);
        _redo->get_changes (redo);
        
        node->add_child_nocopy (*undo);
        node->add_child_nocopy (*redo);

	return *node;
}
