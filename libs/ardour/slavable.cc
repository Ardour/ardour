/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <vector>

#include <glibmm/threads.h>

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/xml++.h"

#include "ardour/slavable.h"
#include "ardour/slavable_automation_control.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "pbd/i18n.h"

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
		child->set_property (X_("number"), *i);
		node->add_child_nocopy (*child);
	}

	return *node;
}

std::vector<boost::shared_ptr<VCA> >
Slavable::masters (VCAManager* manager) const
{
	std::vector<boost::shared_ptr<VCA> > rv;
	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	for (std::set<uint32_t>::const_iterator i = _masters.begin(); i != _masters.end(); ++i) {
		rv.push_back (manager->vca_by_number (*i));
	}
	return rv;
}

bool
Slavable::assigned_to (VCAManager* manager, boost::shared_ptr<VCA> mst) const
{
	if (mst.get () == this) {
		return true;
	}
	std::vector<boost::shared_ptr<VCA> > ml = mst->masters (manager);
	for (std::vector<boost::shared_ptr<VCA> >::const_iterator i = ml.begin (); i != ml.end(); ++i) {
		if (assigned_to (manager, *i)) {
			return true;
		}
	}
	return false;
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
			uint32_t n;
			if ((*i)->get_property (X_("number"), n)) {
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

	if (!vcas.empty()) {

		for (std::vector<boost::shared_ptr<VCA> >::iterator v = vcas.begin(); v != vcas.end(); ++v) {
			assign (*v);
		}

		SlavableControlList scl = slavables ();
		for (SlavableControlList::iterator i = scl.begin(); i != scl.end(); ++i) {
				(*i)->use_saved_master_ratios ();
		}
	}

	assign_connection.disconnect ();

	return 0;
}

void
Slavable::assign (boost::shared_ptr<VCA> v)
{
	assert (v);
	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		if (assign_controls (v)) {
			_masters.insert (v->number());
		}

		/* Do NOT use ::unassign() because it will store a
		 * boost::shared_ptr<VCA> in the functor, leaving a dangling ref to the
		 * VCA.
		 */


		v->Drop.connect_same_thread (unassign_connections, boost::bind (&Slavable::weak_unassign, this, boost::weak_ptr<VCA>(v)));
		v->DropReferences.connect_same_thread (unassign_connections, boost::bind (&Slavable::weak_unassign, this, boost::weak_ptr<VCA>(v)));
	}

	AssignmentChange (v, true);
}

void
Slavable::weak_unassign (boost::weak_ptr<VCA> v)
{
	boost::shared_ptr<VCA> sv (v.lock());
	if (sv) {
		unassign (sv);
	}
}

void
Slavable::unassign (boost::shared_ptr<VCA> v)
{
	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);

		unassign_controls (v);
		if (v) {
			_masters.erase (v->number());
		} else {
			_masters.clear ();
		}
	}
	AssignmentChange (v, false);
}

bool
Slavable::assign_controls (boost::shared_ptr<VCA> vca)
{
	bool rv = false;
	SlavableControlList scl = slavables ();
	for (SlavableControlList::iterator i = scl.begin(); i != scl.end(); ++i) {
		rv |= assign_control (vca, *i);
	}
	return rv;
}

void
Slavable::unassign_controls (boost::shared_ptr<VCA> vca)
{
	SlavableControlList scl = slavables ();
	for (SlavableControlList::iterator i = scl.begin(); i != scl.end(); ++i) {
		unassign_control (vca, *i);
	}
}

bool
Slavable::assign_control (boost::shared_ptr<VCA> vca, boost::shared_ptr<SlavableAutomationControl> slave)
{
	boost::shared_ptr<AutomationControl> master;
	master = vca->automation_control (slave->parameter());
	if (!master) {
		return false;
	}
	slave->add_master (master);
	return true;
}

void
Slavable::unassign_control (boost::shared_ptr<VCA> vca, boost::shared_ptr<SlavableAutomationControl> slave)
{
	if (!vca) {
		/* unassign from all */
		slave->clear_masters ();
	} else {
		boost::shared_ptr<AutomationControl> master;
		master = vca->automation_control (slave->parameter());
		if (master) {
			slave->remove_master (master);
		}
	}
}
