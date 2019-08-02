/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "clock_group.h"

ClockGroup::ClockGroup ()
	: ignore_changes (false)
	, _clock_mode (AudioClock::Samples)
{
}

ClockGroup::~ClockGroup()
{
}

void
ClockGroup::add (AudioClock& clock)
{
	if (clocks.insert (&clock).second) {
		clock.mode_changed.connect (sigc::bind (sigc::mem_fun (*this, &ClockGroup::one_clock_changed), &clock));
		clock.set_mode (_clock_mode);
	}
}

void
ClockGroup::remove (AudioClock& clock)
{
	clocks.erase (&clock);
}

void
ClockGroup::one_clock_changed (AudioClock* clock)
{
	if (!ignore_changes) {
		set_clock_mode (clock->mode());
	}
}

void
ClockGroup::set_clock_mode (AudioClock::Mode mode)
{
	_clock_mode = mode;

	ignore_changes = true;
	for (std::set<AudioClock*>::iterator c = clocks.begin(); c != clocks.end(); ++c) {
		(*c)->set_mode (mode);
	}
	ignore_changes = false;
}

