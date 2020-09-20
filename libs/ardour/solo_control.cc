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
#include "ardour/solo_control.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

#warning NUTEMPO QUESTION what time domain shoudl this really use?
SoloControl::SoloControl (Session& session, std::string const & name, Soloable& s, Muteable& m)
	: SlavableAutomationControl (session, SoloAutomation, ParameterDescriptor (SoloAutomation),
	                             boost::shared_ptr<AutomationList>(new AutomationList(Evoral::Parameter(SoloAutomation), Temporal::AudioTime)),
	                             name)
	, _soloable (s)
	, _muteable (m)
	, _self_solo (false)
	, _soloed_by_others_upstream (0)
	, _soloed_by_others_downstream (0)
	, _transition_into_solo (false)
{
	_list->set_interpolation (Evoral::ControlList::Discrete);
	/* solo changes must be synchronized by the process cycle */
	set_flag (Controllable::RealTime);
}

void
SoloControl::set_self_solo (bool yn)
{
	DEBUG_TRACE (DEBUG::Solo, string_compose ("%1: set SELF solo => %2\n", name(), yn));
	_self_solo = yn;
	set_mute_master_solo ();

	_transition_into_solo = 0;

	if (yn) {
		if (get_masters_value() == 0) {
			_transition_into_solo = 1;
		}
	} else {
		if (get_masters_value() == 0) {
			_transition_into_solo = -1;
		}
	}
}

void
SoloControl::set_mute_master_solo ()
{
	_muteable.mute_master()->set_soloed_by_self (self_soloed() || get_masters_value());

	if (Config->get_solo_control_is_listen_control()) {
		_muteable.mute_master()->set_soloed_by_others (false);
	} else {
		_muteable.mute_master()->set_soloed_by_others (soloed_by_others_downstream() || soloed_by_others_upstream() || get_masters_value());
	}
}

void
SoloControl::mod_solo_by_others_downstream (int32_t delta)
{
	if (_soloable.is_safe() || !can_solo()) {
		return;
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 mod solo-by-downstream by %2, current up = %3 down = %4\n",
						  name(), delta, _soloed_by_others_upstream, _soloed_by_others_downstream));

	if (delta < 0) {
		if (_soloed_by_others_downstream >= (uint32_t) abs (delta)) {
			_soloed_by_others_downstream += delta;
		} else {
			_soloed_by_others_downstream = 0;
		}
	} else {
		_soloed_by_others_downstream += delta;
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 SbD delta %2 = %3\n", name(), delta, _soloed_by_others_downstream));

	set_mute_master_solo ();
	_transition_into_solo = 0;
	Changed (false, Controllable::UseGroup); /* EMIT SIGNAL */
}

void
SoloControl::mod_solo_by_others_upstream (int32_t delta)
{
	if (_soloable.is_safe() || !can_solo()) {
		return;
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 mod solo-by-upstream by %2, current up = %3 down = %4\n",
	                                          name(), delta, _soloed_by_others_upstream, _soloed_by_others_downstream));

	uint32_t old_sbu = _soloed_by_others_upstream;

	if (delta < 0) {
		if (_soloed_by_others_upstream >= (uint32_t) abs (delta)) {
			_soloed_by_others_upstream += delta;
		} else {
			_soloed_by_others_upstream = 0;
		}
	} else {
		_soloed_by_others_upstream += delta;
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose (
		             "%1 SbU delta %2 = %3 old = %4 sbd %5 ss %6 exclusive %7\n",
		             name(), delta, _soloed_by_others_upstream, old_sbu,
		             _soloed_by_others_downstream, _self_solo, Config->get_exclusive_solo()));


	/* push the inverse solo change to everything that feeds us.

	   This is important for solo-within-group. When we solo 1 track out of N that
	   feed a bus, that track will cause mod_solo_by_upstream (+1) to be called
	   on the bus. The bus then needs to call mod_solo_by_downstream (-1) on all
	   tracks that feed it. This will silence them if they were audible because
	   of a bus solo, but the newly soloed track will still be audible (because
	   it is self-soloed).

	   but .. do this only when we are being told to solo-by-upstream (i.e delta = +1),
		   not in reverse.
	*/

	if ((_self_solo || _soloed_by_others_downstream) &&
	    ((old_sbu == 0 && _soloed_by_others_upstream > 0) ||
	     (old_sbu > 0 && _soloed_by_others_upstream == 0))) {

		if (delta > 0 || !Config->get_exclusive_solo()) {
			_soloable.push_solo_upstream (delta);
		}
	}

	set_mute_master_solo ();
	_transition_into_solo = 0;
	Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
}

void
SoloControl::actually_set_value (double val, PBD::Controllable::GroupControlDisposition group_override)
{
	if (_soloable.is_safe() || !can_solo()) {
		return;
	}

	set_self_solo (val == 1.0);

	/* this sets the Evoral::Control::_user_value for us, which will
	   be retrieved by AutomationControl::get_value (), and emits Changed
	*/

	SlavableAutomationControl::actually_set_value (val, group_override);
}

double
SoloControl::get_value () const
{
	if (slaved()) {
		return self_soloed() || get_masters_value ();
	}

	if (_list && boost::dynamic_pointer_cast<AutomationList>(_list)->automation_playback()) {
		// Playing back automation, get the value from the list
		return AutomationControl::get_value();
	}

	return soloed();
}

void
SoloControl::clear_all_solo_state ()
{
	bool change = false;

	if (self_soloed()) {
		PBD::info << string_compose (_("Cleared Explicit solo: %1\n"), name()) << endmsg;
		actually_set_value (0.0, Controllable::NoGroup);
		change = true;
	}

	if (_soloed_by_others_upstream) {
		PBD::info << string_compose (_("Cleared upstream solo: %1 up:%2\n"), name(), _soloed_by_others_upstream)
		          << endmsg;
		_soloed_by_others_upstream = 0;
		change = true;
	}

	if (_soloed_by_others_downstream) {
		PBD::info << string_compose (_("Cleared downstream solo: %1 down:%2\n"), name(), _soloed_by_others_downstream)
		          << endmsg;
		_soloed_by_others_downstream = 0;
		change = true;
	}

	_transition_into_solo = 0; /* Session does not need to propagate */

	if (change) {
		Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
	}
}

int
SoloControl::set_state (XMLNode const & node, int version)
{
	if (SlavableAutomationControl::set_state(node, version)) {
		return -1;
	}

	bool yn;
	if (node.get_property ("self-solo", yn)) {
		set_self_solo (yn);
	}

	uint32_t val;
	if (node.get_property ("soloed-by-upstream", val)) {
		_soloed_by_others_upstream = 0; // needed for mod_.... () to work
		mod_solo_by_others_upstream (val);
	}

	if (node.get_property ("soloed-by-downstream", val)) {
		_soloed_by_others_downstream = 0; // needed for mod_.... () to work
		mod_solo_by_others_downstream (val);
	}

	return 0;
}

XMLNode&
SoloControl::get_state ()
{
	XMLNode& node (SlavableAutomationControl::get_state());

	node.set_property (X_("self-solo"), _self_solo);
	node.set_property (X_("soloed-by-upstream"), _soloed_by_others_upstream);
	node.set_property (X_("soloed-by-downstream"), _soloed_by_others_downstream);

	return node;
}

void
SoloControl::master_changed (bool /*from self*/, GroupControlDisposition, boost::weak_ptr<AutomationControl> wm)
{
	boost::shared_ptr<AutomationControl> m = wm.lock ();
	assert (m);
	bool send_signal = false;

	_transition_into_solo = 0;

	/* Notice that we call get_boolean_masters() BEFORE we call
	 * update_boolean_masters_records(), in order to know what
	 * our master state was BEFORE it gets changed.
	 */


	if (m->get_value()) {
		/* this master is now enabled */
		if (!self_soloed() && get_boolean_masters() == 0) {
			/* not self-soloed, wasn't soloed by masters before */
			send_signal = true;
			_transition_into_solo = 1;
		}
	} else {
		if (!self_soloed() && get_boolean_masters() == 1) {
			/* not self-soloed, soloed by just 1 master before */
			_transition_into_solo = -1;
			send_signal = true;
		}
	}

	update_boolean_masters_records (m);

	if (send_signal) {
		set_mute_master_solo ();
		Changed (false, Controllable::UseGroup);
	}

}

void
SoloControl::post_add_master (boost::shared_ptr<AutomationControl> m)
{
	if (m->get_value()) {

		/* boolean masters records are not updated until AFTER
		 * ::post_add_master() is called, so we can use them to check
		 * on whether any master was already enabled before the new
		 * one was added.
		 */

		if (!self_soloed() && !get_boolean_masters()) {
			_transition_into_solo = 1;
			Changed (false, Controllable::NoGroup);
		}
	}
}

void
SoloControl::pre_remove_master (boost::shared_ptr<AutomationControl> m)
{
	if (!m) {
		/* null control ptr means we're removing all masters. Nothing
		 * to do. Changed will be emitted in
		 * SlavableAutomationControl::clear_masters()
		 */
		return;
	}

	if (m->get_value()) {
		if (!self_soloed() && (get_boolean_masters() == 1)) {
			/* we're not self-soloed, this master is, and we're
			   removing
			   it. SlavableAutomationControl::remove_master() will
			   ensure that we reset our own value after actually
			   removing the master, so that our state does not
			   change (this is a precondition of the
			   SlavableAutomationControl API). This will emit
			   Changed(), and we need to make sure that any
			   listener knows that there has been no transition.
			*/
			_transition_into_solo = 0;
		} else {
			_transition_into_solo = 1;
		}
	} else {
		_transition_into_solo = 0;
	}
}

bool
SoloControl::can_solo () const
{
  if (Config->get_solo_control_is_listen_control()) {
		return _soloable.can_monitor ();
	} else {
		return _soloable.can_solo ();
	}
}
