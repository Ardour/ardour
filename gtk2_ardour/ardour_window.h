/*
    Copyright (C) 2002-2011 Paul Davis

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

#ifndef __ardour_window_h__
#define __ardour_window_h__

#include <gtkmm/window.h>

#include "gtkmm2ext/visibility_tracker.h"

#include "ardour/session_handle.h"

namespace WM {
	class ProxyTemporary;
}

/**
 * This virtual parent class is so that each window uses the
 * same mechanism to declare its closing. It shares a common
 * method of connecting and disconnecting from a Session with
 * all other objects that have a handle on a Session.
 */
class ArdourWindow : public Gtk::Window, public ARDOUR::SessionHandlePtr, public Gtkmm2ext::VisibilityTracker
{
public:
	ArdourWindow (std::string title);
	ArdourWindow (Gtk::Window& parent, std::string title);
	virtual ~ArdourWindow();

protected:
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);
	bool on_delete_event (GdkEventAny *);
	bool on_key_press_event (GdkEventKey*);
	void on_unmap ();

private:
	void init ();
};

#endif // __ardour_window_h__

