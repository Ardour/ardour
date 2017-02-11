/*
    Copyright (C) 2016 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __libardour_slavable_automation_control_h__
#define __libardour_slavable_automation_control_h__

#include "pbd/enumwriter.h"
#include "pbd/error.h"
#include "pbd/i18n.h"

#include "ardour/slavable_automation_control.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

SlavableAutomationControl::SlavableAutomationControl(ARDOUR::Session& s,
                                                     const Evoral::Parameter&                  parameter,
                                                     const ParameterDescriptor&                desc,
                                                     boost::shared_ptr<ARDOUR::AutomationList> l,
                                                     const std::string&                        name,
                                                     Controllable::Flag                        flags)
	: AutomationControl (s, parameter, desc, l, name, flags)
	, _masters_node (0)
{
}

SlavableAutomationControl::~SlavableAutomationControl ()
{
	if (_masters_node) {
		delete _masters_node;
		_masters_node = 0;
	}
}

double
SlavableAutomationControl::get_masters_value_locked () const
{
	double v = _desc.normal;

	if (_desc.toggled) {
		for (Masters::const_iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
			if (mr->second.master()->get_value()) {
				return _desc.upper;
			}
		}
		return _desc.lower;
	}

	for (Masters::const_iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
		/* get current master value, scale by our current ratio with that master */
		v *= mr->second.master()->get_value () * mr->second.ratio();
	}

	return min ((double) _desc.upper, v);
}

double
SlavableAutomationControl::get_value_locked() const
{
	/* read or write masters lock must be held */

	if (_masters.empty()) {
		return Control::get_double (false, _session.transport_frame());
	}

	if (_desc.toggled) {
		/* for boolean/toggle controls, if this slave OR any master is
		 * enabled, this slave is enabled. So check our own value
		 * first, because if we are enabled, we can return immediately.
		 */
		if (Control::get_double (false, _session.transport_frame())) {
			return _desc.upper;
		}
	}

	return get_masters_value_locked ();
}

/** Get the current effective `user' value based on automation state */
double
SlavableAutomationControl::get_value() const
{
	bool from_list = _list && boost::dynamic_pointer_cast<AutomationList>(_list)->automation_playback();

	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	if (!from_list) {
		return get_value_locked ();
	} else {
		return get_masters_value_locked () * Control::get_double (from_list, _session.transport_frame());
	}
}

void
SlavableAutomationControl::actually_set_value (double val, Controllable::GroupControlDisposition group_override)
{
	val = std::max (std::min (val, (double)_desc.upper), (double)_desc.lower);

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);

		if (!_masters.empty()) {
			recompute_masters_ratios (val);
		}
	}

	/* this sets the Evoral::Control::_user_value for us, which will
	   be retrieved by AutomationControl::get_value ()
	*/
	AutomationControl::actually_set_value (val, group_override);
}

void
SlavableAutomationControl::add_master (boost::shared_ptr<AutomationControl> m, bool loading)
{
	std::pair<Masters::iterator,bool> res;

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		const double current_value = get_value_locked ();

		/* ratio will be recomputed below */

		pair<PBD::ID,MasterRecord> newpair (m->id(), MasterRecord (m, 1.0));
		res = _masters.insert (newpair);

		if (res.second) {

			if (!loading) {
				recompute_masters_ratios (current_value);
			}

			/* note that we bind @param m as a weak_ptr<AutomationControl>, thus
			   avoiding holding a reference to the control in the binding
			   itself.
			*/

			m->DropReferences.connect_same_thread (masters_connections, boost::bind (&SlavableAutomationControl::master_going_away, this, boost::weak_ptr<AutomationControl>(m)));

			/* Store the connection inside the MasterRecord, so
			   that when we destroy it, the connection is destroyed
			   and we no longer hear about changes to the
			   AutomationControl.

			   Note that this also makes it safe to store a
			   boost::shared_ptr<AutomationControl> in the functor,
			   since we know we will destroy the functor when the
			   connection is destroyed, which happens when we
			   disconnect from the master (for any reason).

			   Note that we fix the "from_self" argument that will
			   be given to our own Changed signal to "false",
			   because the change came from the master.
			*/

			m->Changed.connect_same_thread (res.first->second.connection, boost::bind (&SlavableAutomationControl::master_changed, this, _1, _2, m));
		}
	}

	if (res.second) {
		/* this will notify everyone that we're now slaved to the master */
		MasterStatusChange (); /* EMIT SIGNAL */
	}

	post_add_master (m);

	update_boolean_masters_records (m);
}

int32_t
SlavableAutomationControl::get_boolean_masters () const
{
	int32_t n = 0;

	if (_desc.toggled) {
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		for (Masters::const_iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
			if (mr->second.yn()) {
				++n;
			}
		}
	}

	return n;
}

void
SlavableAutomationControl::update_boolean_masters_records (boost::shared_ptr<AutomationControl> m)
{
	if (_desc.toggled) {
		/* We may modify a MasterRecord, but we not modify the master
		 * map, so we use a ReaderLock
		 */
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		Masters::iterator mi = _masters.find (m->id());
		if (mi != _masters.end()) {
			/* update MasterRecord to show whether the master is
			   on/off. We need to store this because the master
			   may change (in the sense of emitting Changed())
			   several times without actually changing the result
			   of ::get_value(). This is a feature of
			   AutomationControls (or even just Controllables,
			   really) which have more than a simple scalar
			   value. For example, the master may be a mute control
			   which can be muted_by_self() and/or
			   muted_by_masters(). When either of those two
			   conditions changes, Changed() will be emitted, even
			   though ::get_value() will return the same value each
			   time (1.0 if either are true, 0.0 if neither is).

			   This provides a way for derived types to check
			   the last known state of a Master when the Master
			   changes. We update it after calling
			   ::master_changed() (though derived types must do
			   this themselves).
			*/
			mi->second.set_yn (m->get_value());
		}
	}
}

void
SlavableAutomationControl::master_changed (bool /*from_self*/, GroupControlDisposition gcd, boost::shared_ptr<AutomationControl> m)
{
	update_boolean_masters_records (m);
	Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
}

void
SlavableAutomationControl::master_going_away (boost::weak_ptr<AutomationControl> wm)
{
	boost::shared_ptr<AutomationControl> m = wm.lock();
	if (m) {
		remove_master (m);
	}
}

void
SlavableAutomationControl::remove_master (boost::shared_ptr<AutomationControl> m)
{
	double current_value;
	double new_value;
	bool masters_left;
	Masters::size_type erased = 0;

	pre_remove_master (m);

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		current_value = get_value_locked ();
		erased = _masters.erase (m->id());
		if (erased && !_session.deletion_in_progress()) {
			recompute_masters_ratios (current_value);
		}
		masters_left = _masters.size ();
		new_value = get_value_locked ();
	}

	if (_session.deletion_in_progress()) {
		/* no reason to care about new values or sending signals */
		return;
	}

	if (erased) {
		MasterStatusChange (); /* EMIT SIGNAL */
	}

	if (new_value != current_value) {
		if (masters_left == 0) {
			/* no masters left, make sure we keep the same value
			   that we had before.
			*/
			actually_set_value (current_value, Controllable::UseGroup);
		}
	}

	/* no need to update boolean masters records, since the MR will have
	 * been removed already.
	 */
}

void
SlavableAutomationControl::clear_masters ()
{
	double current_value;
	double new_value;
	bool had_masters = false;

	/* null ptr means "all masters */
	pre_remove_master (boost::shared_ptr<AutomationControl>());

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		current_value = get_value_locked ();
		if (!_masters.empty()) {
			had_masters = true;
		}
		_masters.clear ();
		new_value = get_value_locked ();
	}

	if (had_masters) {
		MasterStatusChange (); /* EMIT SIGNAL */
	}

	if (new_value != current_value) {
		actually_set_value (current_value, Controllable::UseGroup);
	}

	/* no need to update boolean masters records, since all MRs will have
	 * been removed already.
	 */
}

bool
SlavableAutomationControl::slaved_to (boost::shared_ptr<AutomationControl> m) const
{
	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	return _masters.find (m->id()) != _masters.end();
}

bool
SlavableAutomationControl::slaved () const
{
	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	return !_masters.empty();
}

void
SlavableAutomationControl::use_saved_master_ratios ()
{
	if (!_masters_node) {
		return;
	}

	Glib::Threads::RWLock::ReaderLock lm (master_lock);

	/* use stored state, do not recompute */

	if (_desc.toggled) {

		XMLNodeList nlist = _masters_node->children();
		XMLNodeIterator niter;

		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			XMLProperty const * id_prop = (*niter)->property (X_("id"));
			if (!id_prop) {
				continue;
			}
			XMLProperty const * yn_prop = (*niter)->property (X_("yn"));
			if (!yn_prop) {
				continue;
			}
			Masters::iterator mi = _masters.find (ID (id_prop->value()));
			if (mi != _masters.end()) {
				mi->second.set_yn (string_is_affirmative (yn_prop->value()));
			}
		}

	} else {

		XMLProperty const * prop = _masters_node->property (X_("ratio"));

		if (prop) {

			gain_t ratio;
			sscanf (prop->value().c_str(), "%g", &ratio);

			for (Masters::iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
				mr->second.reset_ratio (ratio);
			}
		} else {
			PBD::error << string_compose (_("programming error: %1"), X_("missing ratio information for control slave"))<< endmsg;
		}
	}

	delete _masters_node;
	_masters_node = 0;

	return;
}


XMLNode&
SlavableAutomationControl::get_state ()
{
	XMLNode& node (AutomationControl::get_state());

	/* store VCA master ratios */

	{
		Glib::Threads::RWLock::ReaderLock lm (master_lock);

		if (!_masters.empty()) {

			XMLNode* masters_node = new XMLNode (X_("masters"));

			if (_desc.toggled) {
				for (Masters::iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
					XMLNode* mnode = new XMLNode (X_("master"));
					mnode->add_property (X_("id"), mr->second.master()->id().to_s());
					mnode->add_property (X_("yn"), mr->second.yn());
					masters_node->add_child_nocopy (*mnode);
				}
			} else {
				XMLNode* masters_node = new XMLNode (X_("masters"));
				/* ratio is the same for all masters, so just store one */
				masters_node->add_property (X_("ratio"), PBD::to_string (_masters.begin()->second.ratio(), std::dec));
			}

			node.add_child_nocopy (*masters_node);
		}
	}

	return node;
}

int
SlavableAutomationControl::set_state (XMLNode const& node, int version)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == X_("masters")) {
			_masters_node = new XMLNode (**niter);
		}
	}

	return AutomationControl::set_state (node, version);
}


#endif /* __libardour_slavable_automation_control_h__ */
