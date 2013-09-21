/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <sys/time.h>
#include <iostream>
#include "canvas/debug.h"

using namespace std;

uint64_t PBD::DEBUG::CanvasItems = PBD::new_debug_bit ("canvasitems");
uint64_t PBD::DEBUG::CanvasItemsDirtied = PBD::new_debug_bit ("canvasitemsdirtied");
uint64_t PBD::DEBUG::CanvasEvents = PBD::new_debug_bit ("canvasevents");
uint64_t PBD::DEBUG::CanvasRender = PBD::new_debug_bit ("canvasrender");

struct timeval ArdourCanvas::epoch;
map<string, struct timeval> ArdourCanvas::last_time;
int ArdourCanvas::render_count;
int ArdourCanvas::render_depth;
int ArdourCanvas::dump_depth;

void
ArdourCanvas::set_epoch ()
{
	gettimeofday (&epoch, 0);
}

void
ArdourCanvas::checkpoint (string group, string message)
{
	struct timeval now;
	gettimeofday (&now, 0);

	now.tv_sec -= epoch.tv_sec;
	now.tv_usec -= epoch.tv_usec;
	if (now.tv_usec < 0) {
		now.tv_usec += 1e6;
		--now.tv_sec;
	}
		
	map<string, struct timeval>::iterator last = last_time.find (group);

	if (last != last_time.end ()) {
		time_t seconds = now.tv_sec - last->second.tv_sec;
		suseconds_t useconds = now.tv_usec - last->second.tv_usec;
		if (useconds < 0) {
			useconds += 1e6;
			--seconds;
		}
		cout << (now.tv_sec + ((double) now.tv_usec / 1e6)) << " [" << (seconds + ((double) useconds / 1e6)) << "]: " << message << "\n";
	} else {
		cout << message << "\n";
	}

	last_time[group] = now;
}

