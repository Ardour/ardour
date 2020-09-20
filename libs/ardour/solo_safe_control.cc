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

#warning NUTEMPO QUESTION what time domain shoudl this really use?
SoloSafeControl::SoloSafeControl (Session& session, std::string const & name)
	: SlavableAutomationControl (session, SoloSafeAutomation, ParameterDescriptor (SoloSafeAutomation),
	                             boost::shared_ptr<AutomationList>(new AutomationList(Evoral::Parameter(SoloSafeAutomation), Temporal::AudioTime)),
	                             name)
	, _solo_safe (false)
{
	_list->set_interpolation(Evoral::ControlList::Discrete);
}

void
SoloSafeControl::actually_set_value (double val, PBD::Controllable::GroupControlDisposition gcd)
{
	_solo_safe = (val ? true : false);

	/* this sets the Evoral::Control::_user_value for us, which will
	   be retrieved by AutomationControl::get_value (), and emits Changed
	*/

	AutomationControl::actually_set_value (val, gcd);
}

double
SoloSafeControl::get_value () const
{
	if (slaved()) {
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		return get_masters_value_locked () ? 1.0 : 0.0;
	}

	if (_list && boost::dynamic_pointer_cast<AutomationList>(_list)->automation_playback()) {
		// Playing back automation, get the value from the list
		return AutomationControl::get_value();
	}

	return _solo_safe ? 1.0 : 0.0;
}

int
SoloSafeControl::set_state (XMLNode const & node, int version)
{
	if (SlavableAutomationControl::set_state(node, version)) {
		return -1;
	}

	node.get_property ("solo-safe", _solo_safe);
	return 0;
}

XMLNode&
SoloSafeControl::get_state ()
{
	XMLNode& node (SlavableAutomationControl::get_state());
	node.set_property (X_("solo-safe"), _solo_safe);
	return node;
}
