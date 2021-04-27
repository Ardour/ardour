/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#include <gtkmm.h>
#include <gtk/gtk.h>
#include <gdk/gdkquartz.h>

#include "gui_thread.h"
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


@implementation ResizeNotificationObject

- (ResizeNotificationObject*) initWithPluginUI: (MacVSTPluginUI*) vstui
{
	self = [ super init ];
	plugin_ui = vstui;
	return self;
}

- (void)viewResized:(NSNotification *)notification
{
	(void) notification; // unused
	plugin_ui->view_resized();
}

@end

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
	low_box.signal_visibility_notify_event ().connect (mem_fun (this, &MacVSTPluginUI::lower_box_visibility_notify));
	low_box.signal_size_request ().connect (mem_fun (this, &MacVSTPluginUI::lower_box_size_request));
	low_box.signal_size_allocate ().connect (mem_fun (this, &MacVSTPluginUI::lower_box_size_allocate));
	low_box.signal_map ().connect (mem_fun (this, &MacVSTPluginUI::lower_box_map));
	low_box.signal_unmap ().connect (mem_fun (this, &MacVSTPluginUI::lower_box_unmap));

	pack_start (low_box, true, true);
	low_box.show ();

	vst->LoadPresetProgram.connect (_program_connection, invalidator (*this), boost::bind (&MacVSTPluginUI::set_program, this), gui_context());

	_ns_view = [[NSView new] retain];

	AEffect* plugin = _vst->state()->plugin;
	plugin->dispatcher (plugin, effEditOpen, 0, 0, _ns_view, 0.0f);
	_idle_connection = Glib::signal_idle().connect (sigc::mem_fun (*this, &MacVSTPluginUI::idle));

	_resize_notifier = [[ResizeNotificationObject alloc] initWithPluginUI:this];
	[[NSNotificationCenter defaultCenter] addObserver:_resize_notifier
		selector:@selector(viewResized:) name:NSViewFrameDidChangeNotification
		object:_ns_view];

	NSArray* subviews = [_ns_view subviews];
	for (unsigned long i = 0; i < [subviews count]; ++i) {
		NSView* subview = [subviews objectAtIndex:i];
		[[NSNotificationCenter defaultCenter] addObserver:_resize_notifier
			selector:@selector(viewResized:) name:NSViewFrameDidChangeNotification
			object:subview];
		break; /* only watch first subview (if any) */
	}
}

MacVSTPluginUI::~MacVSTPluginUI ()
{
	[[NSNotificationCenter defaultCenter] removeObserver:_resize_notifier];
	[_resize_notifier release];

	[_ns_view removeFromSuperview];
	[_ns_view release];

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
	NSEvent* nsevent = gdk_quartz_event_get_nsevent ((GdkEvent*)ev);

	if (_ns_view && nsevent) {
		/* filter on nsevent type here because GDK massages FlagsChanged
		 * messages into GDK_KEY_{PRESS,RELEASE} but Cocoa won't
		 * handle a FlagsChanged message as a keyDown or keyUp
		 */
		if ([nsevent type] == NSKeyDown) {
			[[[_ns_view window] firstResponder] keyDown:nsevent];
		} else if ([nsevent type] == NSKeyUp) {
			[[[_ns_view window] firstResponder] keyUp:nsevent];
		} else if ([nsevent type] == NSFlagsChanged) {
			[[[_ns_view window] firstResponder] flagsChanged:nsevent];
		}
	}
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
	[view addSubview:_ns_view];
	low_box.queue_resize ();
}

bool
MacVSTPluginUI::lower_box_visibility_notify (GdkEventVisibility* ev)
{
	return false;
}
void
MacVSTPluginUI::lower_box_map ()
{
	[_ns_view setHidden:0];
}

void
MacVSTPluginUI::lower_box_unmap ()
{
	[_ns_view setHidden:1];
}

void
MacVSTPluginUI::lower_box_size_request (GtkRequisition* requisition)
{
	struct ERect* er = NULL;
	AEffect* plugin = _vst->state()->plugin;
	plugin->dispatcher (plugin, effEditGetRect, 0, 0, &er, 0 );
	if (er) {
		requisition->width  = er->right - er->left;
		requisition->height = er->bottom - er->top;
	} else {
		requisition->width  = 600;
		requisition->height = 400;
	}
}

void
MacVSTPluginUI::lower_box_size_allocate (Gtk::Allocation& allocation)
{
	gint xx, yy;
	gtk_widget_translate_coordinates(
			GTK_WIDGET(low_box.gobj()),
			GTK_WIDGET(low_box.get_parent()->gobj()),
			8, 6, &xx, &yy);
	[_ns_view setFrame:NSMakeRect (xx, yy, allocation.get_width (), allocation.get_height ())];
	NSArray* subviews = [_ns_view subviews];
	for (unsigned long i = 0; i < [subviews count]; ++i) {
		NSView* subview = [subviews objectAtIndex:i];
		[subview setFrame:NSMakeRect (0, 0, allocation.get_width (), allocation.get_height ())];
		break; /* only resize first subview */
	}
}

void
MacVSTPluginUI::view_resized ()
{
	low_box.queue_resize ();
}

int
MacVSTPluginUI::get_XID ()
{
	return _vst->state()->xid; // unused
}

bool
MacVSTPluginUI::idle ()
{
	AEffect* plugin = _vst->state()->plugin;
	_vst->state()->wantIdle = plugin->dispatcher (plugin, effEditIdle, 0, 0, NULL, 0);
	return true; // _vst->state()->wantIdle;
}

void
MacVSTPluginUI::set_program ()
{
	vststate_maybe_set_program (_vst->state());
}
