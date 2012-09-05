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
#include "ardour/profile.h"

#include "ardour_window.h"
#include "gui_thread.h"
#include "soundgrid.h"

using namespace PBD;
using std::cerr;

static NSWindow* sg_window = 0;
static PBD::ScopedConnection sg_connection;

static void
soundgrid_shutdown ()
{
        /* clean up the window that we allocated */

        if (sg_window) {
                [sg_window release];
                sg_window = 0;
        }

        sg_connection.disconnect ();
}

static bool
soundgrid_driver_init (uint32_t max_track_inputs, uint32_t max_track_outputs,
                       uint32_t max_phys_inputs, uint32_t max_phys_outputs)
{
        if (ARDOUR::SoundGrid::instance().configure_driver (max_track_inputs + max_phys_inputs, max_track_outputs + max_phys_outputs) == 0) {
                if (ARDOUR::SoundGrid::instance().map_io_as_jack_ports (max_phys_inputs, max_phys_inputs)) {
                        error << "Failed to map " << max_phys_inputs << " inputs and " 
                              << max_phys_outputs << " outputs as JACK ports"
                              << endmsg;
                }
        }

        return false; /* do not call again */
}

int
soundgrid_init (uint32_t max_track_inputs, uint32_t max_track_outputs,
                uint32_t max_phys_inputs, uint32_t max_phys_outputs)
{
        /* create a new window that we don't display (at least not as
           of August 2012, but we can give it to the SoundGrid library
           which wants one (but also doesn't use it).
        */

        sg_window = [[NSWindow alloc] initWithContentRect:NSZeroRect 
                                                       styleMask:NSTitledWindowMask 
                                                         backing:NSBackingStoreBuffered 
                                                           defer:1];

        [sg_window setReleasedWhenClosed:0];
        [sg_window retain];

        ARDOUR::SoundGrid::Shutdown.connect (sg_connection, MISSING_INVALIDATOR, soundgrid_shutdown, gui_context());

        if (ARDOUR::SoundGrid::instance().initialize ([sg_window contentView], max_track_inputs, max_track_outputs,
                                                      max_phys_inputs, max_phys_inputs)) {
        
                [sg_window release];
                sg_window = 0;

                return -1;
        }

        /* as of early August 2012, we need to wait 2 seconds before configuring the CoreAudio driver */

        Glib::signal_timeout().connect (sigc::bind (sigc::ptr_fun (soundgrid_driver_init), 
                                                    max_track_inputs, max_track_outputs,
                                                    max_phys_inputs, max_phys_outputs), 2000);

        /* tell everyone/everything that we're using soundgrid */

        ARDOUR::Profile->set_soundgrid ();

        return 0;
}

#endif
