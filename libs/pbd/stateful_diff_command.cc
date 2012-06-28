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
#include "pbd/property_list.h"
#include "pbd/demangle.h"
#include "i18n.h"

using namespace std;
using namespace PBD;

/** Create a new StatefulDiffCommand by examining the changes made to a Stateful
 *  since the last time that clear_changes was called on it.
 *  @param s Stateful object.
 */

StatefulDiffCommand::StatefulDiffCommand (boost::shared_ptr<StatefulDestructible> s)
        : _object (s)
        , _changes (0)
{
	_changes = s->get_changes_as_properties (this);

        /* if the stateful object that this command refers to goes away,
           be sure to notify owners of this command.
        */

        s->DropReferences.connect_same_thread (*this, boost::bind (&Destructible::drop_references, this));
}

StatefulDiffCommand::StatefulDiffCommand (boost::shared_ptr<StatefulDestructible> s, XMLNode const & n)
	: _object (s)
        , _changes (0)
{
        const XMLNodeList& children (n.children());

        for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
                if ((*i)->name() == X_("Changes")) {
                        _changes = s->property_factory (**i);
                }
	}

        assert (_changes != 0);

        /* if the stateful object that this command refers to goes away,
           be sure to notify owners of this command.
        */

        s->DropReferences.connect_same_thread (*this, boost::bind (&Destructible::drop_references, this));
}

StatefulDiffCommand::~StatefulDiffCommand ()
{
        drop_references ();

        delete _changes;
}

void
StatefulDiffCommand::operator() ()
{
	boost::shared_ptr<Stateful> s (_object.lock());

	if (s) {
                s->apply_changes (*_changes);
	}
}

void
StatefulDiffCommand::undo ()
{
	boost::shared_ptr<Stateful> s (_object.lock());

	if (s) {
		PropertyList p = *_changes;
		p.invert ();
                s->apply_changes (p);
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

        XMLNode* changes = new XMLNode (X_("Changes"));

        _changes->get_changes_as_xml (changes);
        
        node->add_child_nocopy (*changes);

	return *node;
}

bool
StatefulDiffCommand::empty () const
{
	return _changes->empty();
}
