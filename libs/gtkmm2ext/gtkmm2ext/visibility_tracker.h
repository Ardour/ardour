/*
 * Copyright (C) 2013-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libgtkmm2ext_visibility_tracker__
#define __libgtkmm2ext_visibility_tracker__

#include <gdk/gdkevents.h>

#include "gtkmm2ext/visibility.h"

namespace GTK {
	class Window;
}

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API VisibilityTracker : public virtual sigc::trackable {
  public:
    VisibilityTracker (Gtk::Window&);
    virtual ~VisibilityTracker() {}

    static void set_use_window_manager_visibility (bool);
    static bool use_window_manager_visibility() { return _use_window_manager_visibility; }
    void cycle_visibility ();

    bool fully_visible() const;
    bool not_visible() const;
    bool partially_visible() const;

    Gtk::Window& window () const { return _window; }

  private:
    Gtk::Window& _window;
    GdkVisibilityState _visibility;

    static bool _use_window_manager_visibility;

    bool handle_visibility_notify_event (GdkEventVisibility*);
};

}

#endif /* __libgtkmm2ext_visibility_tracker__ */
