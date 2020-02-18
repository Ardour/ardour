/*
 * Copyright (C) 2014 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#ifndef TIMERS_H
#define TIMERS_H

#include <sigc++/sigc++.h>

namespace Timers
{

sigc::connection blink_connect(const sigc::slot<void,bool>& slot);

sigc::connection second_connect(const sigc::slot<void>& slot);

sigc::connection rapid_connect(const sigc::slot<void>& slot);

sigc::connection super_rapid_connect(const sigc::slot<void>& slot);

void set_fps_interval(unsigned int interval);

sigc::connection fps_connect(const sigc::slot<void>& slot);

class TimerSuspender {
	public:
		TimerSuspender ();
		~TimerSuspender ();
};

};

#endif // TIMERS_H
