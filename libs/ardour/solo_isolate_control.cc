/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include "ardour/debug.h"
#include "ardour/mute_master.h"
#include "ardour/session.h"
#include "ardour/solo_isolate_control.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

SoloIsolateControl::SoloIsolateControl (Session& session, std::string const & name, Soloable& s, Muteable& m)
	: SlavableAutomationControl (session, SoloIsolateAutomation, ParameterDescriptor (SoloIsolateAutomation),
	                             boost::shared_ptr<AutomationList>(new AutomationList(Evoral::Parameter(SoloIsolateAutomation), Temporal::AudioTime)),
	                             name)
	, _soloable (s)
	, _solo_isolated (false)
	, _solo_isolated_by_upstream (0)
{
	_list->set_interpolation (Evoral::ControlList::Discrete);
	/* isolate changes must be synchronized by the process cycle */
	set_flag (Controllable::RealTime);
}

void
SoloIsolateControl::master_changed (bool from_self, PBD::Controllable::GroupControlDisposition gcd, boost::weak_ptr<AutomationControl>)
{
	if (!_soloable.can_solo()) {
		return;
	}

	bool master_soloed;

	{
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		master_soloed = (bool) get_masters_value_locked ();
	}

	/* Master is considered equivalent to an upstream solo control, not
	 * direct control over self-soloed.
	 */

	mod_solo_isolated_by_upstream (master_soloed ? 1 : -1);

	/* no need to call AutomationControl::master_changed() since it just
	   emits Changed() which we already did in mod_solo_by_others_upstream()
	*/
}

void
SoloIsolateControl::mod_solo_isolated_by_upstream (int32_t delta)
{
	bool old = solo_isolated ();
	DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 mod_solo_isolated_by_upstream cur: %2 d: %3\n",
	                                          name(), _solo_isolated_by_upstream, delta));

	if (delta < 0) {
		if (_solo_isolated_by_upstream >= (uint32_t) abs(delta)) {
			_solo_isolated_by_upstream += delta;
		} else {
			_solo_isolated_by_upstream = 0;
		}
	} else {
		_solo_isolated_by_upstream += delta;
	}

	if (solo_isolated() != old) {
		Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
	}
}

void
SoloIsolateControl::actually_set_value (double val, PBD::Controllable::GroupControlDisposition gcd)
{
	if (!_soloable.can_solo()) {
		return;
	}

	set_solo_isolated (val, gcd);

	/* this sets the Evoral::Control::_user_value for us, which will
	   be retrieved by AutomationControl::get_value (), and emits Changed
	*/

	AutomationControl::actually_set_value (val, gcd);
}

void
SoloIsolateControl::set_solo_isolated (bool yn, Controllable::GroupControlDisposition group_override)
{
	if (!_soloable.can_solo()) {
		return;
	}

	bool changed = false;

	if (yn) {
		if (_solo_isolated == false) {
			changed = true;
		}
		_solo_isolated = true;
	} else {
		if (_solo_isolated == true) {
			_solo_isolated = false;
			changed = true;
		}
	}

	if (!changed) {
		return;
	}

	_soloable.push_solo_isolate_upstream (yn ? 1 : -1);

	/* XXX should we back-propagate as well? (April 2010: myself and chris goddard think not) */

	Changed (true, group_override); /* EMIT SIGNAL */
}


double
SoloIsolateControl::get_value () const
{
	if (slaved()) {
		return solo_isolated() || get_masters_value ();
	}

	if (_list && boost::dynamic_pointer_cast<AutomationList>(_list)->automation_playback()) {
		// Playing back automation, get the value from the list
		return AutomationControl::get_value();
	}

	return solo_isolated ();
}

int
SoloIsolateControl::set_state (XMLNode const & node, int version)
{
	if (SlavableAutomationControl::set_state(node, version)) {
		return -1;
	}

	node.get_property ("solo-isolated", _solo_isolated);
	return 0;
}

XMLNode&
SoloIsolateControl::get_state ()
{
	XMLNode& node (SlavableAutomationControl::get_state());
	node.set_property (X_("solo-isolated"), _solo_isolated);
	return node;
}
