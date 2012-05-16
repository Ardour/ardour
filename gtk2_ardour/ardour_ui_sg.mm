/*
    Copyright (C) 2012 Paul Davis

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

#include "ardour/soundgrid.h"

#include "ardour_ui.h"
#include "public_editor.h"

#ifdef __APPLE__
#define Marker MarkerStupidApple
#include <gdk/gdkquartz.h>
#undef Marker

#ifdef check
#undef check
#endif
#ifdef NO
#undef NO
#endif
#ifdef YES
#undef YES
#endif
#endif

#ifdef USE_SOUNDGRID

void
ARDOUR_UI::soundgrid_init ()
{
#ifdef __APPLE__
        NSView* nsview = gdk_quartz_window_get_nsview (editor->get_window()->gobj());
        ARDOUR::SoundGrid::instance().initialize (nsview);
#endif
}

#endif
