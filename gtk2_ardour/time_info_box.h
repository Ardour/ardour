/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __time_info_box_h__
#define __time_info_box_h__

#include <map>

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>

#include "gtkmm2ext/cairo_packer.h"

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

#include "ardour_button.h"

namespace ARDOUR {
	class Session;
        class Location;
}

class AudioClock;

class TimeInfoBox : public CairoHPacker, public ARDOUR::SessionHandlePtr
{
  public:
    TimeInfoBox ();
    ~TimeInfoBox ();

    void set_session (ARDOUR::Session*);

  private:
    Gtk::Table left;
    Gtk::Table right;

    AudioClock* selection_start;
    AudioClock* selection_end;
    AudioClock* selection_length;
    
    AudioClock* punch_start;
    AudioClock* punch_end;

    Gtk::Label selection_title;
    Gtk::Label punch_title;
    bool syncing_selection;
    bool syncing_punch;

    void punch_changed (ARDOUR::Location*);
    void punch_location_changed (ARDOUR::Location*);
    void watch_punch (ARDOUR::Location*);
    PBD::ScopedConnectionList punch_connections;
    PBD::ScopedConnectionList editor_connections;

    ArdourButton punch_in_button;
    ArdourButton punch_out_button;

    void selection_changed ();

    void sync_selection_mode (AudioClock*);
    void sync_punch_mode (AudioClock*);

    bool clock_button_release_event (GdkEventButton* ev, AudioClock* src);
    void track_mouse_mode ();
};


#endif /* __time_info_box_h__ */
