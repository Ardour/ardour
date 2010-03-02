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
#include "i18n.h"

using namespace std;
using namespace PBD;

/** Create a new StatefulDiffCommand by examining the changes made to a Stateful
 *  since the last time that clear_history was called on it.
 *  @param s Stateful object.
 */

StatefulDiffCommand::StatefulDiffCommand (boost::shared_ptr<Stateful> s)
	: _object (s)
        , _before (new PropertyList)
        , _after (new PropertyList)
{
        s->diff (*_before, *_after);
}

StatefulDiffCommand::StatefulDiffCommand (boost::shared_ptr<Stateful> s, XMLNode const & n)
	: _object (s)
        , _before (0)
        , _after (0)
{
        const XMLNodeList& children (n.children());

        for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
                if ((*i)->name() == X_("Undo")) {
                        _before = s->property_factory (**i);
                } else if ((*i)->name() == X_("Do")) {
                        _after = s->property_factory (**i);
                }
        }

        assert (_before != 0);
        assert (_after != 0);
}

StatefulDiffCommand::~StatefulDiffCommand ()
{
        delete _before;
        delete _after;
}

void
StatefulDiffCommand::operator() ()
{
	boost::shared_ptr<Stateful> s (_object.lock());

	if (s) {
                PropertyChange changed = s->set_properties (*_after);
                if (!changed.empty()) {
                        s->PropertyChanged (changed);
                }
	}
}

void
StatefulDiffCommand::undo ()
{
	boost::shared_ptr<Stateful> s (_object.lock());

	if (s) {
                std::cerr << "Undoing a stateful diff command\n";
                PropertyChange changed = s->set_properties (*_before);
                if (!changed.empty()) {
                        std::cerr << "Sending changed\n";
                        s->PropertyChanged (changed);
                }
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
	node->add_property ("type-name", typeid(*s.get()).name());

        XMLNode* before = new XMLNode (X_("Undo"));
        XMLNode* after = new XMLNode (X_("Do"));

        _before->add_history_state (before);
        _after->add_history_state (after);
        
        node->add_child_nocopy (*before);
        node->add_child_nocopy (*after);

	return *node;
}
