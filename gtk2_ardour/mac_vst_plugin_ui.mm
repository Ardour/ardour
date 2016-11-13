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

#include <gtkmm.h>
#include <gtk/gtk.h>
#include <gdk/gdkquartz.h>

#include "ardour/plugin_insert.h"
#include "ardour/mac_vst_plugin.h"
#include "ardour/vst_types.h"
#include "mac_vst_plugin_ui.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

struct ERect{
    short top;
    short left;
    short bottom;
    short right;
};

VSTPluginUI*
create_mac_vst_gui (boost::shared_ptr<PluginInsert> insert)
{
	/* PluginUIWindow::create_mac_vst_editor assures this cast works */
	boost::shared_ptr<MacVSTPlugin> mvst =  boost::dynamic_pointer_cast<MacVSTPlugin> (insert->plugin());
	return new MacVSTPluginUI (insert, mvst);
}


MacVSTPluginUI::MacVSTPluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<VSTPlugin> vst)
	: VSTPluginUI (pi, vst)
	, _ns_view (0)
{
	low_box.add_events (Gdk::VISIBILITY_NOTIFY_MASK | Gdk::EXPOSURE_MASK);
	low_box.signal_realize().connect (mem_fun (this, &MacVSTPluginUI::lower_box_realized));
	pack_start (low_box, true, true);
	low_box.show ();
}

MacVSTPluginUI::~MacVSTPluginUI ()
{
	if (_ns_view) {
		[_ns_view removeFromSuperview];
		[_ns_view release];
	}

	AEffect* plugin = _vst->state()->plugin;
	plugin->dispatcher (plugin, effEditClose, 0, 0, 0, 0.0f);
	_idle_connection.disconnect();
}

NSWindow*
MacVSTPluginUI::get_nswindow ()
{
	Gtk::Container* toplevel = get_toplevel();
	if (!toplevel || !toplevel->is_toplevel()) {
		error << _("MacVSTPluginUI: no top level window!") << endmsg;
		return 0;
	}
	NSWindow* true_parent = gdk_quartz_window_get_nswindow (toplevel->get_window()->gobj());

	if (!true_parent) {
		error << _("MacVSTPluginUI: no top level window!") << endmsg;
		return 0;
	}

	return true_parent;
}

int
MacVSTPluginUI::package (Gtk::Window& win)
{
	VSTPluginUI::package (win);
	return 0;
}

void
MacVSTPluginUI::forward_key_event (GdkEventKey* ev)
{
}

void
MacVSTPluginUI::lower_box_realized ()
{
	NSWindow* win = get_nswindow ();
	if (!win) {
		return;
	}

	[win setAutodisplay:1]; // turn off GTK stuff for this window

	NSView* view = gdk_quartz_window_get_nsview (low_box.get_window()->gobj());
	_ns_view = [[NSView new] retain];
	[view addSubview:_ns_view];

	AEffect* plugin = _vst->state()->plugin;
	plugin->dispatcher (plugin, effEditOpen, 0, 0, _ns_view, 0.0f);

	struct ERect* er = NULL;
	plugin->dispatcher (plugin, effEditGetRect, 0, 0, &er, 0 );
	if (er) {
		int req_width = er->right - er->left;
		int req_height = er->bottom - er->top;

		low_box.set_size_request (req_width, req_height);

		gint xx, yy;
		gtk_widget_translate_coordinates(
				GTK_WIDGET(low_box.gobj()),
				GTK_WIDGET(low_box.get_parent()->gobj()),
				8, 6, &xx, &yy);
		[_ns_view setFrame:NSMakeRect(xx, yy, req_width, req_height)];
	}

	_idle_connection = Glib::signal_idle().connect (sigc::mem_fun (*this, &MacVSTPluginUI::idle));
}

int
MacVSTPluginUI::get_XID ()
{
	return _vst->state()->xid;
}

bool
MacVSTPluginUI::idle ()
{
	AEffect* plugin = _vst->state()->plugin;
	_vst->state()->wantIdle = plugin->dispatcher (plugin, effEditIdle, 0, 0, NULL, 0);
	return true; // _vst->state()->wantIdle;
}
