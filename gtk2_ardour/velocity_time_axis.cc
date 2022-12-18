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

#include "velocity_time_axis.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;

VelocityTimeAxisView::VelocityTimeAxisView (
	Session* s,
	boost::shared_ptr<Stripable> strip,
	boost::shared_ptr<Automatable> a,
	boost::shared_ptr<AutomationControl> c,
	PublicEditor& e,
	TimeAxisView& parent,
	bool show_regions,
	ArdourCanvas::Canvas& canvas,
	const string & nom,
	const string & nomparent
	)

	: AutomationTimeAxisView (s, strip, a, c, Evoral::Parameter (MidiVelocityAutomation), e, parent, show_regions, canvas, nom, nomparent)
{
}

VelocityTimeAxisView::~VelocityTimeAxisView()
{
}

