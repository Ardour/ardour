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

    $Id$
*/

#include <fst.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include <ardour/insert.h>
#include <ardour/vst_plugin.h>

#include "plugin_ui.h"
#include "prompter.h"

#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;

VSTPluginUI::VSTPluginUI (PluginInsert& pi, VSTPlugin& vp)
	: PlugUIBase (pi),
	  vst (vp)
{
	fst_run_editor (vst.fst());

	preset_box.pack_end (bypass_button, false, false, 10);
	preset_box.pack_end (save_button, false, false);
	preset_box.pack_end (combo, false, false);

	bypass_button.set_active (!insert.active());
	
	pack_start (preset_box, false, false);
	pack_start (socket, true, true);
}

VSTPluginUI::~VSTPluginUI ()
{
	// nothing to do here - plugin destructor destroys the GUI
}

int
VSTPluginUI::get_preferred_height ()
{
	return vst.fst()->height;
}

int
VSTPluginUI::package (Gtk::Window& win)
{
	/* for GTK+2, remove this: you cannot add to a realized socket */

	socket.realize ();

	/* forward configure events to plugin window */

	win.signal_configure_event.connect (bind (mem_fun (*this, &VSTPluginUI::configure_handler), socket.gobj()));

	/* XXX in GTK2, use add_id() instead of steal, although add_id()
	   assumes that the window's owner understands the XEmbed protocol.
	*/
	
	socket.steal (fst_get_XID (vst.fst()));

	return 0;
}

gboolean
VSTPluginUI::configure_handler (GdkEventConfigure* ev, GtkSocket *socket)
{
	XEvent event;

	gint x, y;

	if (socket->plug_window == NULL) {
		return FALSE;
	}

	event.xconfigure.type = ConfigureNotify;
	event.xconfigure.event = GDK_WINDOW_XWINDOW (socket->plug_window);
	event.xconfigure.window = GDK_WINDOW_XWINDOW (socket->plug_window);

	/* The ICCCM says that synthetic events should have root relative
	 * coordinates. We still aren't really ICCCM compliant, since
	 * we don't send events when the real toplevel is moved.
	 */
	gdk_error_trap_push ();
	gdk_window_get_origin (socket->plug_window, &x, &y);
	gdk_error_trap_pop ();

	event.xconfigure.x = x;
	event.xconfigure.y = y;
	event.xconfigure.width = GTK_WIDGET(socket)->allocation.width;
	event.xconfigure.height = GTK_WIDGET(socket)->allocation.height;

	event.xconfigure.border_width = 0;
	event.xconfigure.above = None;
	event.xconfigure.override_redirect = False;

	gdk_error_trap_push ();
	XSendEvent (GDK_WINDOW_XDISPLAY (socket->plug_window),
		    GDK_WINDOW_XWINDOW (socket->plug_window),
		    False, StructureNotifyMask, &event);
	// gdk_display_sync (GDK_WINDOW_XDISPLAY (socket->plug_window));
	gdk_error_trap_pop ();

	return FALSE;
}

