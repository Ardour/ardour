/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <gtkmm.h>
#include <gtk/gtk.h>
#include <gdk/gdkquartz.h>

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/unwind.h"

#include "ardour/plugin_insert.h"
#include "ardour/vst3_plugin.h"

#include "gtkmm2ext/gui_thread.h"

#include "gui_thread.h"
#include "vst3_nsview_plugin_ui.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace Steinberg;

VST3PluginUI*
create_mac_vst3_gui (boost::shared_ptr<PluginInsert> plugin_insert, Gtk::VBox** box)
{
	VST3NSViewPluginUI* v = new VST3NSViewPluginUI (plugin_insert, boost::dynamic_pointer_cast<VST3Plugin> (plugin_insert->plugin()));
	*box = v;
	return v;
}


VST3NSViewPluginUI::VST3NSViewPluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<VST3Plugin> vst3)
	: VST3PluginUI (pi, vst3)
{
	pack_start (_gui_widget, true, true);

	_gui_widget.add_events (Gdk::VISIBILITY_NOTIFY_MASK | Gdk::EXPOSURE_MASK);
	_gui_widget.signal_realize().connect (mem_fun (this, &VST3NSViewPluginUI::view_realized));
	_gui_widget.signal_visibility_notify_event ().connect (mem_fun (this, &VST3NSViewPluginUI::view_visibility_notify));
	_gui_widget.signal_size_request ().connect (mem_fun (this, &VST3NSViewPluginUI::view_size_request));
	_gui_widget.signal_size_allocate ().connect (mem_fun (this, &VST3NSViewPluginUI::view_size_allocate));
	_gui_widget.signal_map ().connect (mem_fun (this, &VST3NSViewPluginUI::view_map));
	_gui_widget.signal_unmap ().connect (mem_fun (this, &VST3NSViewPluginUI::view_unmap));
	_gui_widget.signal_scroll_event ().connect (sigc::mem_fun (*this, &VST3NSViewPluginUI::forward_scroll_event), false);

	//vst->LoadPresetProgram.connect (_program_connection, invalidator (*this), boost::bind (&VST3NSViewPluginUI::set_program, this), gui_context());

	_ns_view = [[NSView new] retain];

	IPlugView* view = _vst3->view ();

	if (!view) {
		printf ("FAILED TO get view.\n");
		return;
	}

	if (kResultOk != view->attached (reinterpret_cast<void*> (_ns_view), Steinberg::kPlatformTypeNSView)) {
		printf ("FAILED TO ATTACH..\n");
	}

	ViewRect rect;
	if (view->getSize (&rect) == kResultOk) {
		_req_width  = rect.right - rect.left;
		_req_height = rect.bottom - rect.top;
	}

	_gui_widget.show ();
}

VST3NSViewPluginUI::~VST3NSViewPluginUI ()
{
	assert (_view_realized);
	_vst3->close_view ();
	[_ns_view removeFromSuperview];
	[_ns_view release];
}

void
VST3NSViewPluginUI::view_size_request (GtkRequisition* requisition)
{
	requisition->width  = _req_width + 8;
	requisition->height = _req_height + 6;
}

void
VST3NSViewPluginUI::view_size_allocate (Gtk::Allocation& allocation)
{
	IPlugView* view = _vst3->view ();
	if (!view || !_view_realized) {
		return;
	}

	gint xx = 0;
	gint yy = 0;
	gtk_widget_translate_coordinates(
			GTK_WIDGET(_gui_widget.gobj()),
			GTK_WIDGET(_gui_widget.get_parent()->gobj()),
			0, 0, &xx, &yy);

	PBD::Unwinder<bool> uw (_resize_in_progress, true);

	ViewRect rect;
	if (view->getSize (&rect) == kResultOk
	    && !(rect.right - rect.left == allocation.get_width () && rect.bottom - rect.top ==  allocation.get_height () && rect.left == xx && rect.top == yy))
	{
		rect.left   = xx;
		rect.top    = yy;
		rect.right  = rect.left + allocation.get_width ();
		rect.bottom = rect.top + allocation.get_height ();
#if 0
		if (view->checkSizeConstraint (&rect) != kResultTrue) {
			view->getSize (&rect);
		}
		allocation.set_width (rect.right - rect.left);
		allocation.set_height (rect.bottom - rect.top);
#endif
		bool can_resize = true;
#if 1 // quirks
		/* Reason reports canResize() == kResultTrue, but crashes in onSize()
		 * reported to reasonstudios.com on 2020-11-19 */
		if (_vst3->unique_id() == "D75F65D1581F203A8D793B3C01583323" /* Reason Rack Plugin */) can_resize = false;
		if (_vst3->unique_id() == "C64E2730B0614A6EAEFC432F9CF6151A" /* Reason Rack Plugin Effect */) can_resize = false;
#endif
		if (can_resize && view->canResize() == kResultTrue) {
			view->onSize (&rect);
		}
	}

	[_ns_view setFrame:NSMakeRect (xx, yy, allocation.get_width (), allocation.get_height ())];
	NSArray* subviews = [_ns_view subviews];
	for (unsigned long i = 0; i < [subviews count]; ++i) {
		NSView* subview = [subviews objectAtIndex:i];
		[subview setFrame:NSMakeRect (0, 0, allocation.get_width (), allocation.get_height ())];
		break; /* only resize first subview */
	}
}

void
VST3NSViewPluginUI::resize_callback (int width, int height)
{
	//printf ("VST3NSViewPluginUI::resize_callback %d x %d\n", width, height);
	IPlugView* view = _vst3->view ();
	if (!view || _resize_in_progress) {
		return;
	}
	if (view->canResize() == kResultTrue) {
		gint xx, yy;
		if (gtk_widget_translate_coordinates (
		    GTK_WIDGET(_gui_widget.gobj()),
		    GTK_WIDGET(get_toplevel()->gobj()),
		    0, 0, &xx, &yy))
		{
			get_window()->resize (width + xx, height + yy);
		}
	} else {
		_req_width  = width;
		_req_height = height;
		_gui_widget.queue_resize ();
	}
}

void
VST3NSViewPluginUI::view_realized ()
{
	NSWindow* win = get_nswindow ();
	if (!win) {
		printf ("NO WINDOW!\n");
		return;
	}

	[win setAutodisplay:1]; // turn off GTK stuff for this window

	NSView* nsview = gdk_quartz_window_get_nsview (_gui_widget.get_window()->gobj());
	[nsview addSubview:_ns_view];
	_view_realized = true;
	_gui_widget.queue_resize ();
}

bool
VST3NSViewPluginUI::view_visibility_notify (GdkEventVisibility* ev)
{
	return false;
}

void
VST3NSViewPluginUI::view_map ()
{
	[_ns_view setHidden:0];
}

void
VST3NSViewPluginUI::view_unmap ()
{
	[_ns_view setHidden:1];
}


bool
VST3NSViewPluginUI::on_window_show (const std::string& /*title*/)
{
	gtk_widget_realize (GTK_WIDGET(_gui_widget.gobj()));
	show_all ();
	_gui_widget.queue_resize ();
	return true;
}

void
VST3NSViewPluginUI::on_window_hide ()
{
	hide_all ();
}

void
VST3NSViewPluginUI::forward_key_event (GdkEventKey* ev)
{
	/* despite the VST3 spec mandating onKeyUp/Down, many
	 * plugins (notably JUCE) reply on platform callbacks.
	 */
	NSEvent* nsevent = gdk_quartz_event_get_nsevent ((GdkEvent*)ev);
	if (!nsevent) {
		return;
	}
	if ([nsevent type] == NSKeyDown) {
		[[[_ns_view window] firstResponder] keyDown:nsevent];
	} else if ([nsevent type] == NSKeyUp) {
		[[[_ns_view window] firstResponder] keyUp:nsevent];
	} else if ([nsevent type] == NSFlagsChanged) {
		[[[_ns_view window] firstResponder] flagsChanged:nsevent];
	}
}

void
VST3NSViewPluginUI::grab_focus ()
{
	[_ns_view becomeFirstResponder];
}

NSWindow*
VST3NSViewPluginUI::get_nswindow ()
{
	Gtk::Container* toplevel = get_toplevel();
	if (!toplevel || !toplevel->is_toplevel()) {
		error << _("VST3NSViewPluginUI: no top level window!") << endmsg;
		return 0;
	}
	NSWindow* true_parent = gdk_quartz_window_get_nswindow (toplevel->get_window()->gobj());

	if (!true_parent) {
		error << _("VST3NSViewPluginUI: no top level window!") << endmsg;
		return 0;
	}

	return true_parent;
}
