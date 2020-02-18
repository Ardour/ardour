/*
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
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


#include "gtkmm2ext/gui_thread.h"
#include "ardour/lxvst_plugin.h"
#include "ardour/linux_vst_support.h"
#include "lxvst_plugin_ui.h"

#include <gdk/gdkx.h> /* must come later than glibmm/object.h */

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
	_resize_connection.disconnect();

	// plugin destructor destroys the custom GUI, via the vstfx engine,
	// and then our PluginUIWindow does the rest
}

void
LXVSTPluginUI::resize_callback ()
{
	void* gtk_parent_window = _vst->state()->gtk_window_parent;

	if (gtk_parent_window) {
		int width  = _vst->state()->width;
		int height = _vst->state()->height;
#ifndef NDEBUG
		printf ("LXVSTPluginUI::resize_callback %d x %d\n", width, height);
#endif
		_socket.set_size_request(
				width  + _vst->state()->hoffset,
				height + _vst->state()->voffset);

		((Gtk::Window*) gtk_parent_window)->resize (width, height + LXVST_H_FIDDLE);
		if (_vst->state()->linux_plugin_ui_window) {
		}
	}
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
	_vst->state()->gtk_window_parent = (void*) (&win);

	/* Map the UI start and stop updating events to 'Map' events on the Window */

	_vst->VSTSizeWindow.connect (_resize_connection, invalidator (*this), boost::bind (&LXVSTPluginUI::resize_callback, this), gui_context());
	return 0;
}

void
LXVSTPluginUI::forward_key_event (GdkEventKey* gdk_key)
{
	if (!_vst->state()->gtk_window_parent) {
		return;
	}

	Glib::RefPtr<Gdk::Window> gdk_window = ((Gtk::Window*) _vst->state()->gtk_window_parent)->get_window();

	if (!gdk_window) {
		return;
	}

	XEvent xev;
	int mask;

	switch (gdk_key->type) {
	case GDK_KEY_PRESS:
		xev.xany.type = KeyPress;
		mask = KeyPressMask;
		break;
	case GDK_KEY_RELEASE:
		xev.xany.type = KeyRelease;
		mask = KeyReleaseMask;
		break;
	default:
		return;
	}

	/* XXX relies on GDK using X11 definitions for these fields */

	xev.xkey.state = gdk_key->state;
	xev.xkey.keycode = gdk_key->hardware_keycode; /* see gdk/x11/gdkevents-x11.c:translate_key_event() */

	xev.xkey.x = 0;
	xev.xkey.y = 0;
	xev.xkey.x_root = 0;
	xev.xkey.y_root = 0;
	xev.xkey.root = gdk_x11_get_default_root_xwindow();
	xev.xkey.window = _vst->state()->linux_plugin_ui_window ? _vst->state()->linux_plugin_ui_window : _vst->state()->xid;
	xev.xkey.subwindow = None;
	xev.xkey.time = gdk_key->time;

	xev.xany.serial = 0; /* we don't have one */
	xev.xany.send_event = true; /* pretend we are using XSendEvent */
	xev.xany.display = GDK_WINDOW_XDISPLAY (gdk_window->gobj());

	if (_vst->state()->eventProc) {
		_vst->state()->eventProc (&xev);
	} else if (!dispatch_effeditkey (gdk_key)) {
		XSendEvent (xev.xany.display, xev.xany.window, TRUE, mask, &xev);
	}
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
		Glib::usleep (1000);
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
