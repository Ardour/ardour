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

#include <fst.h>
#include <gtk/gtksocket.h>
#include <ardour/processor.h>
#include <ardour/vst_plugin.h>

#include "plugin_ui.h"

#include <gdk/gdkx.h>

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

VSTPluginUI::VSTPluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<VSTPlugin> vp)
	: PlugUIBase (pi),
	  vst (vp)
{
	fst_run_editor (vst->fst());

	preset_box.pack_end (bypass_button, false, false, 10);
	preset_box.pack_end (save_button, false, false);
	preset_box.pack_end (preset_combo, false, false);

	bypass_button.set_active (!insert->active());
	
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
	return vst->fst()->height;
}

int
VSTPluginUI::get_preferred_width ()
{
	return vst->fst()->width;
}

int
VSTPluginUI::package (Gtk::Window& win)
{
	/* forward configure events to plugin window */

	win.signal_configure_event().connect (bind (mem_fun (*this, &VSTPluginUI::configure_handler), &socket), false);

	/*
	   this assumes that the window's owner understands the XEmbed protocol.
	*/
	
	socket.add_id (fst_get_XID (vst->fst()));

	return 0;
}

bool
VSTPluginUI::configure_handler (GdkEventConfigure* ev, Gtk::Socket *socket)
{
	XEvent event;
	gint x, y;
	GdkWindow* w;

	if (socket == 0 || ((w = socket->gobj()->plug_window) == 0)) {
		return false;
	}

	event.xconfigure.type = ConfigureNotify;
	event.xconfigure.event = GDK_WINDOW_XWINDOW (w);
	event.xconfigure.window = GDK_WINDOW_XWINDOW (w);

	/* The ICCCM says that synthetic events should have root relative
	 * coordinates. We still aren't really ICCCM compliant, since
	 * we don't send events when the real toplevel is moved.
	 */
	gdk_error_trap_push ();
	gdk_window_get_origin (w, &x, &y);
	gdk_error_trap_pop ();

	event.xconfigure.x = x;
	event.xconfigure.y = y;
	event.xconfigure.width = GTK_WIDGET(socket->gobj())->allocation.width;
	event.xconfigure.height = GTK_WIDGET(socket->gobj())->allocation.height;

	event.xconfigure.border_width = 0;
	event.xconfigure.above = None;
	event.xconfigure.override_redirect = False;

	gdk_error_trap_push ();
	XSendEvent (GDK_WINDOW_XDISPLAY (w), GDK_WINDOW_XWINDOW (w), False, StructureNotifyMask, &event);
	gdk_error_trap_pop ();

	return false;
}

