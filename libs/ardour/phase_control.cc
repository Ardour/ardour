/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/phase_control.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

PhaseControl::PhaseControl (Session& session, std::string const & name)
	: AutomationControl (session, PhaseAutomation, ParameterDescriptor (PhaseAutomation),
	                     boost::shared_ptr<AutomationList>(new AutomationList(Evoral::Parameter(PhaseAutomation), Temporal::AudioTime)),
	                     name)
{
}

void
PhaseControl::actually_set_value (double val, Controllable::GroupControlDisposition gcd)
{
	_phase_invert = boost::dynamic_bitset<> (std::numeric_limits<double>::digits, (unsigned long) val);

	AutomationControl::actually_set_value (val, gcd);
}

/** @param c Audio channel index.
 *  @param yn true to invert phase, otherwise false.
 */
void
PhaseControl::set_phase_invert (uint32_t c, bool yn)
{
	if (_phase_invert[c] != yn) {
		_phase_invert[c] = yn;
		AutomationControl::actually_set_value (_phase_invert.to_ulong(), Controllable::NoGroup);
	}
}

void
PhaseControl::set_phase_invert (boost::dynamic_bitset<> p)
{
	if (_phase_invert != p) {
		_phase_invert = p;
		AutomationControl::actually_set_value (_phase_invert.to_ulong(), Controllable::NoGroup);
	}
}

void
PhaseControl::resize (uint32_t n)
{
	_phase_invert.resize (n);
}

XMLNode&
PhaseControl::get_state ()
{
	XMLNode& node (AutomationControl::get_state ());

	string p;
	boost::to_string (_phase_invert, p);
	node.set_property ("phase-invert", p);

	return node;
}

int
PhaseControl::set_state (XMLNode const & node, int version)
{
	AutomationControl::set_state (node, version);

	std::string str;
	if (node.get_property (X_("phase-invert"), str)) {
		set_phase_invert (boost::dynamic_bitset<> (str));
	}

	return 0;
}
