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
#include <gdk/gdk.h>
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

const char*
ArdourCanvas::event_type_string (int event_type)
{
	switch (event_type) {
	case GDK_NOTHING:
		return "nothing";
	case GDK_DELETE:
		return "delete";
	case GDK_DESTROY:
		return "destroy";
	case GDK_EXPOSE:
		return "expose";
	case GDK_MOTION_NOTIFY:
		return "motion_notify";
	case GDK_BUTTON_PRESS:
		return "button_press";
	case GDK_2BUTTON_PRESS:
		return "2button_press";
	case GDK_3BUTTON_PRESS:
		return "3button_press";
	case GDK_BUTTON_RELEASE:
		return "button_release";
	case GDK_KEY_PRESS:
		return "key_press";
	case GDK_KEY_RELEASE:
		return "key_release";
	case GDK_ENTER_NOTIFY:
		return "enter_notify";
	case GDK_LEAVE_NOTIFY:
		return "leave_notify";
	case GDK_FOCUS_CHANGE:
		return "focus_change";
	case GDK_CONFIGURE:
		return "configure";
	case GDK_MAP:
		return "map";
	case GDK_UNMAP:
		return "unmap";
	case GDK_PROPERTY_NOTIFY:
		return "property_notify";
	case GDK_SELECTION_CLEAR:
		return "selection_clear";
	case GDK_SELECTION_REQUEST:
		return "selection_request";
	case GDK_SELECTION_NOTIFY:
		return "selection_notify";
	case GDK_PROXIMITY_IN:
		return "proximity_in";
	case GDK_PROXIMITY_OUT:
		return "proximity_out";
	case GDK_DRAG_ENTER:
		return "drag_enter";
	case GDK_DRAG_LEAVE:
		return "drag_leave";
	case GDK_DRAG_MOTION:
		return "drag_motion";
	case GDK_DRAG_STATUS:
		return "drag_status";
	case GDK_DROP_START:
		return "drop_start";
	case GDK_DROP_FINISHED:
		return "drop_finished";
	case GDK_CLIENT_EVENT:
		return "client_event";
	case GDK_VISIBILITY_NOTIFY:
		return "visibility_notify";
	case GDK_NO_EXPOSE:
		return "no_expose";
	case GDK_SCROLL:
		return "scroll";
	case GDK_WINDOW_STATE:
		return "window_state";
	case GDK_SETTING:
		return "setting";
	case GDK_OWNER_CHANGE:
		return "owner_change";
	case GDK_GRAB_BROKEN:
		return "grab_broken";
	case GDK_DAMAGE:
		return "damage";
	}

	return "unknown";
}
