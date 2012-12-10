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
#include <gtkmm/messagedialog.h>
#include <gtkmm/progressbar.h>

#include "gtkmm2ext/gtk_ui.h"

#include "pbd/compose.h"

#include "ardour/debug.h"
#include "ardour/profile.h"

#include "ardour_window.h"
#include "gui_thread.h"
#include "soundgrid.h"

#include "i18n.h"

using namespace PBD;
using std::cerr;

static NSWindow* sg_window = 0;
static Gtk::MessageDialog* wait_dialog = 0;
static Gtk::ProgressBar* pbar = 0;
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
soundgrid_driver_init (uint32_t max_phys_inputs, uint32_t max_phys_outputs, uint32_t max_tracks)
{
        wait_dialog->set_secondary_text (_("Configuring CoreAudio driver ..."), true);
        pbar->hide ();

        Gtkmm2ext::UI::instance()->flush_pending ();

        ARDOUR::SoundGrid::instance().configure_driver (max_phys_inputs, max_phys_outputs, max_tracks);

        /* end the wait dialog */

        wait_dialog->response (0);

        return false; /* do not call again */
}

static bool
soundgrid_initialize (uint32_t max_tracks, uint32_t max_busses, 
                      uint32_t max_phys_inputs, uint32_t max_phys_outputs,
                      uint32_t max_plugins)
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
        
        if (ARDOUR::SoundGrid::instance().initialize ([sg_window contentView], 
                                                      max_tracks, max_busses, 
                                                      max_phys_inputs,
                                                      max_phys_outputs,
                                                      max_plugins)) {
                
                [sg_window release];
                sg_window = 0;

        } else {

                /* as of early August 2012, we need to wait 5 seconds before configuring the CoreAudio driver */
                
                Glib::signal_timeout().connect (sigc::bind (sigc::ptr_fun (soundgrid_driver_init), 
                                                            max_phys_inputs, max_phys_outputs, max_tracks), 2500);
                
                /* tell everyone/everything that we're using soundgrid */
                
                ARDOUR::Profile->set_soundgrid ();
        }

        return false; /* do not call again */
}

static bool
pulse_pbar ()
{
        pbar->pulse();
        return true;
}

int
soundgrid_init (uint32_t max_phys_inputs, uint32_t max_phys_outputs, 
                uint32_t max_tracks, uint32_t max_busses,
                uint32_t max_plugins)
{
        if (ARDOUR::SoundGrid::initialized()) {
                return 0;
        }

        Glib::signal_idle().connect (sigc::bind (sigc::ptr_fun (soundgrid_initialize), 
                                                 max_tracks, max_busses, 
                                                 max_phys_inputs,
                                                 max_phys_outputs,
                                                 max_plugins));

        
        wait_dialog = new Gtk::MessageDialog (_("Please wait a few seconds while your SoundGrid is configured"),
                                              false, /* use markup */
                                              Gtk::MESSAGE_WARNING,
                                              Gtk::BUTTONS_NONE, 
                                              true); /* modal */

        wait_dialog->set_position (Gtk::WIN_POS_CENTER);
        wait_dialog->set_title (_("SoundGrid Initializing ..."));

        pbar = manage (new Gtk::ProgressBar);
        sigc::connection pulse_connection;

        pbar->set_size_request (100, -1);
        wait_dialog->get_vbox()->pack_start (*pbar, false, false);
        pbar->show ();

        pulse_connection = Glib::signal_timeout().connect (sigc::ptr_fun (pulse_pbar), 250);

        wait_dialog->run ();
        
        pulse_connection.disconnect ();

        delete wait_dialog;

        return 0;
}

#endif
