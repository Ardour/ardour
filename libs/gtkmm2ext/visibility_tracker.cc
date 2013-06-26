/*
    Copyright (C) 2013 Paul Davis

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
#include <iostream>
#include <gtkmm/window.h>

#include "gtkmm2ext/visibility_tracker.h"

using namespace Gtkmm2ext;

VisibilityTracker::VisibilityTracker (Gtk::Window& win)
	: _window (win)
	, _visibility (GdkVisibilityState (0))
{
	_window.add_events (Gdk::VISIBILITY_NOTIFY_MASK);
	_window.signal_visibility_notify_event().connect (sigc::mem_fun (*this, &VisibilityTracker::handle_visibility_notify_event));
}

bool
VisibilityTracker::handle_visibility_notify_event (GdkEventVisibility* ev)
{
	_visibility = ev->state;
	// std::cerr << "VT: " << _window.get_title() << " vis event, fv = " << fully_visible() << " pv = " << partially_visible() << " nv = " << not_visible() << std::endl;
	return false;
}

void
VisibilityTracker::cycle_visibility ()
{
	if (fully_visible ()) {
		_window.hide ();
	} else {
		_window.present ();
	}
}

bool
VisibilityTracker::fully_visible () const
{
	return _window.is_mapped() && (_visibility == GDK_VISIBILITY_UNOBSCURED);
}

bool
VisibilityTracker::not_visible () const
{
	return !_window.is_mapped() || (_visibility == GDK_VISIBILITY_FULLY_OBSCURED);
}

bool
VisibilityTracker::partially_visible () const
{
	return _window.is_mapped() && ((_visibility == GDK_VISIBILITY_PARTIAL) || (_visibility == GDK_VISIBILITY_UNOBSCURED));
}
