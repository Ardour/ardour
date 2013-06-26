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

#ifndef __libgtkmm2ext_visibility_tracker__
#define __libgtkmm2ext_visibility_tracker__

#include <gdk/gdkevents.h>

namespace GTK {
	class Window;
}

namespace Gtkmm2ext {

class VisibilityTracker : public virtual sigc::trackable {
  public:
    VisibilityTracker (Gtk::Window&);
    virtual ~VisibilityTracker() {}
    
    void cycle_visibility ();

    bool fully_visible() const;
    bool not_visible() const;
    bool partially_visible() const;

    Gtk::Window& window () const { return _window; }

  private:
    Gtk::Window& _window;
    GdkVisibilityState _visibility;
    bool handle_visibility_notify_event (GdkEventVisibility*);
};

}

#endif /* __libgtkmm2ext_visibility_tracker__ */ 
