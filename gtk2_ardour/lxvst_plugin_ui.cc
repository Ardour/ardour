/*
    Copyright (C) 2004 Paul Davis 

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

#include "ardour/lxvst_plugin.h"
#include "ardour/linux_vst_support.h"
#include "lxvst_plugin_ui.h"
#include "ardour_ui.h"
#include <gdk/gdkx.h>

#define LXVST_H_FIDDLE 40

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

LXVSTPluginUI::LXVSTPluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<VSTPlugin> lxvp)
	: VSTPluginUI (pi, lxvp)
{
	vstfx_run_editor (_vst->state ());
}

LXVSTPluginUI::~LXVSTPluginUI ()
{
	_screen_update_connection.disconnect();	
	
	// plugin destructor destroys the custom GUI, via the vstfx engine,
	// and then our PluginUIWindow does the rest
}


bool
LXVSTPluginUI::start_updating (GdkEventAny*)
{
	_screen_update_connection.disconnect();
	_screen_update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect (mem_fun(*this, &LXVSTPluginUI::resize_callback));
	return false;
}

bool
LXVSTPluginUI::stop_updating (GdkEventAny*)
{
	_screen_update_connection.disconnect();
	return false;
}


void
LXVSTPluginUI::resize_callback ()
{
	/* We could maybe use this to resize the plugin GTK parent window
	   if required
	*/
	
	if (!_vst->state()->want_resize) {
		return;
	}

	int new_height = _vst->state()->height;
	int new_width = _vst->state()->width;
	
	void* gtk_parent_window = _vst->state()->extra_data;
	
	if (gtk_parent_window) {
		((Gtk::Window*) gtk_parent_window)->resize (new_width, new_height + LXVST_H_FIDDLE);
	}
	
	_vst->state()->want_resize = 0;
}

int
LXVSTPluginUI::get_preferred_height ()
{	
	/* XXX: FIXME */
	
	/* We have to return the required height of the plugin UI window +  a fiddle factor
	   because we can't know how big the preset menu bar is until the window is realised
	   and we can't realise it until we have told it how big we would like it to be
	   which we can't do until it is realised etc
	*/

	// May not be 40 for all screen res etc	
	return VSTPluginUI::get_preferred_height () + LXVST_H_FIDDLE;
}

int
LXVSTPluginUI::package (Gtk::Window& win)
{
	VSTPluginUI::package (win);
	
	/* Map the UI start and stop updating events to 'Map' events on the Window */
	
	win.signal_map_event().connect (mem_fun (*this, &LXVSTPluginUI::start_updating));
	win.signal_unmap_event().connect (mem_fun (*this, &LXVSTPluginUI::stop_updating));

	_vst->state()->extra_data = (void*) (&win);
	_vst->state()->want_resize = 0;

	return 0;
}

void
LXVSTPluginUI::forward_key_event (GdkEventKey*)
{
	std::cerr << "LXVSTPluginUI : keypress forwarding to linuxVSTs unsupported" << std::endl;
}

int
LXVSTPluginUI::get_XID ()
{
	/* Wait for the lock to become free - otherwise
	   the window might be in the process of being
	   created and we get bad Window errors when trying
	   to embed it in the GTK UI
	*/
	
	pthread_mutex_lock (&(_vst->state()->lock));
	
	/* The Window may be scheduled for creation
	   but not actually created by the gui_event_loop yet - 
	   spin here until it has been activated.  Possible
	   deadlock if the window never gets activated but
	   should not be called here if the window doesn't
	   exist or will never exist
	*/
	
	while (!(_vst->state()->been_activated)) {
		usleep (1000);
	}
	
	int const id = _vst->state()->xid;
	
	pthread_mutex_unlock (&(_vst->state()->lock));
	
	/* Finally it might be safe to return the ID - 
	   problems will arise if we return either a zero ID
	   and GTK tries to socket it or if we return an ID
	   which hasn't yet become real to the server
	*/
	
	return id;
}

typedef int (*error_handler_t)( Display *, XErrorEvent *);
static Display *the_gtk_display;
static error_handler_t vstfx_error_handler;
static error_handler_t gtk_error_handler;

static int 
gtk_xerror_handler (Display*, XErrorEvent*)
{
	std::cerr << "** ERROR ** LXVSTPluginUI : Trapped an X Window System Error" << std::endl;
	
	return 0;
}

void
gui_init (int* argc, char** argv[])
{
	vstfx_error_handler = XSetErrorHandler (NULL);
	gtk_init (argc, argv);
	the_gtk_display = gdk_x11_display_get_xdisplay (gdk_display_get_default());
	gtk_error_handler = XSetErrorHandler (gtk_xerror_handler);
}

