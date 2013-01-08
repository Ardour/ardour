/*
    Copyright (C) 2000-2010 Paul Davis

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

#include "ardour/vst_plugin.h"
#include "ardour/vst_types.h"
#include "vst_plugin_ui.h"
#include <gdk/gdkx.h>

VSTPluginUI::VSTPluginUI (boost::shared_ptr<ARDOUR::PluginInsert> insert, boost::shared_ptr<ARDOUR::VSTPlugin> plugin)
	: PlugUIBase (insert)
	, _vst (plugin)
{
	Gtk::HBox* box = manage (new Gtk::HBox);
	box->set_spacing (6);
	box->set_border_width (6);
	box->pack_end (focus_button, false, false);
	box->pack_end (bypass_button, false, false, 10);
	box->pack_end (delete_button, false, false);
	box->pack_end (save_button, false, false);
	box->pack_end (add_button, false, false);
	box->pack_end (_preset_combo, false, false);

	bypass_button.set_active (!insert->active ());

	pack_start (*box, false, false);
	pack_start (_socket, true, true);
}

VSTPluginUI::~VSTPluginUI ()
{

}

void
VSTPluginUI::preset_selected ()
{
	_socket.grab_focus ();
	PlugUIBase::preset_selected ();
}

int
VSTPluginUI::get_preferred_height ()
{
	return _vst->state()->height;
}

int
VSTPluginUI::get_preferred_width ()
{
	return _vst->state()->width;
}

int
VSTPluginUI::package (Gtk::Window& win)
{
	/* Forward configure events to plugin window */
	win.signal_configure_event().connect (sigc::mem_fun (*this, &VSTPluginUI::configure_handler), false);

	/* This assumes that the window's owner understands the XEmbed protocol */
	_socket.add_id (get_XID ());
	
	return 0;
}

bool
VSTPluginUI::configure_handler (GdkEventConfigure*)
{
	XEvent event;
	gint x, y;
	GdkWindow* w;

	if ((w = _socket.gobj()->plug_window) == 0) {
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
	event.xconfigure.width = GTK_WIDGET (_socket.gobj())->allocation.width;
	event.xconfigure.height = GTK_WIDGET (_socket.gobj())->allocation.height;

	event.xconfigure.border_width = 0;
	event.xconfigure.above = None;
	event.xconfigure.override_redirect = False;

	gdk_error_trap_push ();
	XSendEvent (GDK_WINDOW_XDISPLAY (w), GDK_WINDOW_XWINDOW (w), False, StructureNotifyMask, &event);
	gdk_error_trap_pop ();

	return false;
}

