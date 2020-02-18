/*
 * Copyright (C) 2011 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_clock_group_h__
#define __gtk_ardour_clock_group_h__

#include <set>
#include <sigc++/signal.h>

#include "audio_clock.h"

class ClockGroup : public sigc::trackable
{
public:
	ClockGroup ();
	~ClockGroup ();

	void set_clock_mode (AudioClock::Mode);
	AudioClock::Mode clock_mode() const { return _clock_mode; }

	void add (AudioClock&);
	void remove (AudioClock&);

private:
	std::set<AudioClock*> clocks;
	bool ignore_changes;
	AudioClock::Mode _clock_mode;

	void one_clock_changed (AudioClock*);
};

#endif /* __gtk_ardour_clock_group_h__ */
