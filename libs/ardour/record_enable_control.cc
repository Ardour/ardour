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
#include "ardour/record_enable_control.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

RecordEnableControl::RecordEnableControl (Session& session, std::string const & name, Recordable& r, Temporal::TimeDomain td)
	: SlavableAutomationControl (session, RecEnableAutomation, ParameterDescriptor (RecEnableAutomation),
	                             boost::shared_ptr<AutomationList>(new AutomationList(Evoral::Parameter(RecEnableAutomation), td)),
	                             name)
	, _recordable (r)
{
	_list->set_interpolation(Evoral::ControlList::Discrete);

	/* record-enable changes must be synchronized by the process cycle */
	set_flag (Controllable::RealTime);
}

void
RecordEnableControl::set_value (double val, Controllable::GroupControlDisposition gcd)
{
	/* Because we are marked as a RealTime control, this will queue
	   up the control change to be executed in a realtime context.
	*/
	SlavableAutomationControl::set_value (val, gcd);
}

void
RecordEnableControl::actually_set_value (double val, Controllable::GroupControlDisposition gcd)
{
	if (val && !_recordable.can_be_record_enabled()) {
		std::cerr << "rec-enable not allowed\n";
		return;
	}

	SlavableAutomationControl::actually_set_value (val, gcd);
}

void
RecordEnableControl::do_pre_realtime_queue_stuff (double newval)
{
	/* do the non-RT part of rec-enabling first - the RT part will be done
	 * on the next process cycle. This does mean that theoretically we are
	 * doing things provisionally on the assumption that the rec-enable
	 * change will work, but this had better be a solid assumption for
	 * other reasons.
	 *
	 * this is guaranteed to be called from a non-process thread.
	 */

	if (_recordable.prep_record_enabled (newval)) {
		/* failed */
		std::cerr << "Prep rec-enable failed\n";
	}
}
