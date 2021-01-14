/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/monitor_control.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

MonitorControl::MonitorControl (Session& session, std::string const & name, Monitorable& m, Temporal::TimeDomain td)
	: SlavableAutomationControl (session, MonitoringAutomation, ParameterDescriptor (MonitoringAutomation),
	                             boost::shared_ptr<AutomationList>(new AutomationList(Evoral::Parameter(MonitoringAutomation), td)),
	                             name)

	, _monitorable (m)
	, _monitoring (MonitorAuto)
{
	_list->set_interpolation(Evoral::ControlList::Discrete);
	/* monitoring changes must be synchronized by the process cycle */
	set_flag (Controllable::RealTime);
}

void
MonitorControl::actually_set_value (double val, Controllable::GroupControlDisposition gcd)
{
	_monitoring = MonitorChoice ((int) val);
	AutomationControl::actually_set_value (val, gcd);
}

XMLNode&
MonitorControl::get_state ()
{
	XMLNode& node (SlavableAutomationControl::get_state());
	node.set_property (X_("monitoring"), _monitoring);
	return node;
}

int
MonitorControl::set_state (XMLNode const & node, int version)
{
	SlavableAutomationControl::set_state (node, version);

	if (!node.get_property (X_("monitoring"), _monitoring)) {
		_monitoring = MonitorAuto;
	}

	return 0;
}
