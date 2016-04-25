/*
    Copyright (C) 2016 Paul Davis

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

#include <vector>

#include <glibmm/threads.h>

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/xml++.h"

#include "ardour/slavable.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "i18n.h"

using namespace PBD;
using namespace ARDOUR;

std::string Slavable::xml_node_name = X_("Slavable");
PBD::Signal1<void,VCAManager*> Slavable::Assign; /* signal sent once
                                                  * assignment is possible */

Slavable::Slavable ()
{
	Assign.connect_same_thread (assign_connection, boost::bind (&Slavable::do_assign, this, _1));
}

XMLNode&
Slavable::get_state () const
{
	XMLNode* node = new XMLNode (xml_node_name);
	XMLNode* child;

	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	for (std::set<uint32_t>::const_iterator i = _masters.begin(); i != _masters.end(); ++i) {
		child = new XMLNode (X_("Master"));
		child->add_property (X_("number"), to_string (*i, std::dec));
		node->add_child_nocopy (*child);
	}

	return *node;
}

int
Slavable::set_state (XMLNode const& node, int version)
{
	if (node.name() != xml_node_name) {
		return -1;
	}

	XMLNodeList const& children (node.children());
	Glib::Threads::RWLock::WriterLock lm (master_lock);

	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == X_("Master")) {
			XMLProperty const* prop = (*i)->property (X_("number"));
			if (prop) {
				uint32_t n = atoi (prop->value());
				_masters.insert (n);
			}
		}
	}

	return 0;
}

int
Slavable::do_assign (VCAManager* manager)
{
	std::vector<boost::shared_ptr<VCA> > vcas;

	{
		Glib::Threads::RWLock::ReaderLock lm (master_lock);

		for (std::set<uint32_t>::const_iterator i = _masters.begin(); i != _masters.end(); ++i) {
			boost::shared_ptr<VCA> v = manager->vca_by_number (*i);
			if (v) {
				vcas.push_back (v);
			} else {
				warning << string_compose (_("Master #%1 not found, assignment lost"), *i) << endmsg;
			}
		}
	}

	/* now that we've released the lock, we can do the assignments */

	for (std::vector<boost::shared_ptr<VCA> >::iterator v = vcas.begin(); v != vcas.end(); ++v) {
		assign (*v);
	}

	assign_connection.disconnect ();

	return 0;
}

void
Slavable::assign (boost::shared_ptr<VCA> v)
{
	Glib::Threads::RWLock::WriterLock lm (master_lock);
	if (assign_controls (v) == 0) {
		_masters.insert (v->number());
	}
}

void
Slavable::unassign (boost::shared_ptr<VCA> v)
{
	Glib::Threads::RWLock::WriterLock lm (master_lock);
	(void) unassign_controls (v);
	_masters.erase (v->number());
}
