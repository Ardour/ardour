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
#include "soundgrid.h"

using namespace PBD;
using std::cerr;

static int sg_status = -1;

static void sg_init ()
{
        DEBUG_TRACE (PBD::DEBUG::SoundGrid, "thread starting SG init\n");

        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSZeroRect 
                                                       styleMask:NSTitledWindowMask 
                                                         backing:NSBackingStoreBuffered 
                                                           defer:1];

        ARDOUR::SoundGrid::instance().set_pool (pool);

        if (ARDOUR::SoundGrid::instance().initialize ([window contentView]) == 0) {
                sg_status = 1;
        } else {
                sg_status = -1;
        }

        cerr << "initialization done, status === " << sg_status << std::endl;
}

#ifdef __APPLE__

@interface DummyThread : NSThread 
{
}
@end

@implementation DummyThread
- (void) main
{
        printf ("dummy thread running\n");
}
@end

#endif

static bool hack (sigc::slot<void> theSlot)
{
        theSlot();
        return true;
}

int
soundgrid_init ()
{
#ifdef __APPLE__
        /* need to ensure that Cocoa is multithreaded */
        if (![NSThread isMultiThreaded]) {
                cerr << "Starting dummy thread\n";
                NSThread* dummy = [[DummyThread alloc] init];
                [dummy start];
        } else {
                cerr << "Already multithreaded\n";
        }
#endif

        ArdourWindow win (_("Initializing SoundGrid"));
        Gtk::Label label (_("Please wait while SoundGrid initializes"));
        Gtk::VBox packer;
        Gtk::ProgressBar progress;

        label.show ();
        progress.show();
        packer.show ();
        packer.pack_start (label);
        packer.pack_start (progress);

        win.set_position (Gtk::WIN_POS_MOUSE);
        win.add (packer);
        win.present ();
                
        DEBUG_TRACE (DEBUG::SoundGrid, "Initializing SoundGrid instance\n");

        sg_status = 0;

        Glib::Thread* thr = Glib::Thread::create (sigc::ptr_fun (sg_init));

        Glib::signal_timeout().connect (sigc::bind (sigc::ptr_fun (hack), sigc::mem_fun (progress, &Gtk::ProgressBar::pulse)), 100);
        
        while (sg_status == 0) {
                gtk_main_iteration ();
        }
        
        win.hide ();

        while (gtk_events_pending ()) {
                gtk_main_iteration ();
        }

        thr->join ();

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SG initialization returned %1\n", sg_status));
        return sg_status;
}

#endif
