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

#ifndef __bigclock_window_h__
#define __bigclock_window_h__

#include "ardour_window.h"

class AudioClock;

class BigClockWindow : public ArdourWindow
{
  public:
    BigClockWindow (AudioClock&);

  private:
    AudioClock& clock;
    bool resize_in_progress;
    int original_height;
    int original_width;
    int original_font_size;

    void on_size_allocate (Gtk::Allocation&);
    void on_realize ();
    void on_unmap ();
    bool on_key_press_event (GdkEventKey*);

    bool text_resizer (int, int);
    void reset_aspect_ratio ();
};

#endif // __ardour_window_h__

