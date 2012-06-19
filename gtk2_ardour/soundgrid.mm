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

#include <iostream>

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

#include <glibmm/thread.h>
#include <gtkmm/label.h>

#include "pbd/compose.h"

#include "ardour/debug.h"

#include "ardour_window.h"
#include "gui_thread.h"
#include "soundgrid.h"

using namespace PBD;
using std::cerr;

static NSWindow* sg_window = 0;
static PBD::ScopedConnection sg_connection;

void
soundgrid_shutdown ()
{
        if (sg_window) {
                [sg_window release];
                sg_window = 0;
        }
        sg_connection.disconnect ();
}

int
soundgrid_init ()
{
        sg_window = [[NSWindow alloc] initWithContentRect:NSZeroRect 
                                                       styleMask:NSTitledWindowMask 
                                                         backing:NSBackingStoreBuffered 
                                                           defer:1];

        [sg_window setReleasedWhenClosed:0];
        [sg_window retain];

        ARDOUR::SoundGrid::Shutdown.connect (sg_connection, MISSING_INVALIDATOR, soundgrid_shutdown, gui_context());

        return ARDOUR::SoundGrid::instance().initialize ([sg_window contentView]);
}

#endif
