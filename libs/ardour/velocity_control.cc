/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cmath>

#include "pbd/convert.h"
#include "pbd/strsplit.h"

#include "ardour/velocity_control.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;


VelocityControl::VelocityControl (Session& session)
	: SlavableAutomationControl (session, Evoral::Parameter (MidiVelocityAutomation), ParameterDescriptor (Evoral::Parameter (MidiVelocityAutomation)),
	                             std::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (MidiVelocityAutomation), Temporal::BeatTime)),
	                             _("Velocity"))
{
}
