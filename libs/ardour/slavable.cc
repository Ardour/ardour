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

#include <glibmm/threads.h>

#include "pbd/convert.h"
#include "pbd/xml++.h"

#include "ardour/slavable.h"
#include "ardour/vca.h"

#include "i18n.h"

using namespace ARDOUR;

std::string Slavable::xml_node_name = X_("Slavable");

Slavable::Slavable ()
{

}

XMLNode&
Slavable::state () const
{
	XMLNode* node = new XMLNode (xml_node_name);
	XMLNode* child;

	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	for (std::set<uint32_t>::const_iterator i = _masters.begin(); i != _masters.end(); ++i) {
		child = new XMLNode (X_("Master"));
		child->add_property (X_("number"), PBD::to_string (*i, std::dec));
		node->add_child_nocopy (*child);
	}

	return *node;
}

int
Slavable::assign (Session& s, XMLNode const& node)
{
	return 0;
}

void
Slavable::assign (boost::shared_ptr<VCA> v)
{
	if (assign_controls (v) == 0) {
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		_masters.insert (v->number());
	}
}

void
Slavable::unassign (boost::shared_ptr<VCA> v)
{
	(void) unassign_controls (v);
	Glib::Threads::RWLock::WriterLock lm (master_lock);
	_masters.erase (v->number());
}
