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

#include "ardour/audioengine.h"
#include "ardour/record_safe_control.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

#warning NUTEMPO question: what is the right time domain here
RecordSafeControl::RecordSafeControl (Session& session, std::string const & name, Recordable& r)
	: SlavableAutomationControl (session, RecSafeAutomation, ParameterDescriptor (RecSafeAutomation),
	                             boost::shared_ptr<AutomationList>(new AutomationList(Evoral::Parameter(RecSafeAutomation), Temporal::AudioTime)),
	                             name)
	, _recordable (r)
{
	_list->set_interpolation(Evoral::ControlList::Discrete);

	/* record-enable changes must be synchronized by the process cycle */
	set_flag (Controllable::RealTime);
}

void
RecordSafeControl::actually_set_value (double val, Controllable::GroupControlDisposition gcd)
{
	if (val && !_recordable.can_be_record_safe()) {
		std::cerr << "rec-enable not allowed\n";
		return;
	}

	SlavableAutomationControl::actually_set_value (val, gcd);
}

