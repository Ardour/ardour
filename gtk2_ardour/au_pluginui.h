/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk2_ardour_auplugin_ui_h__
#define __gtk2_ardour_auplugin_ui_h__

#include <vector>
#include <string>

#include <stdint.h>

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <AudioUnit/AudioUnitCarbonView.h>
#include <AudioUnit/AudioUnit.h>

/* fix up stupid apple macros */

#undef check
#undef require
#undef verify

#ifdef YES
#undef YES
#endif
#ifdef NO
#undef NO
#endif

#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/combobox.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include "plugin_ui.h"

namespace ARDOUR {
	class AUPlugin;
	class PluginInsert;
	class IOProcessor;
}

class AUPluginUI;

@interface NotificationObject : NSObject {
	@private
		AUPluginUI* plugin_ui;
		NSWindow* cocoa_parent;
		NSWindow* top_level_parent;
}
@end

@interface LiveResizeNotificationObject : NSObject {
	@private
		AUPluginUI* plugin_ui;
}
@end

class AUPluginUI : public PlugUIBase, public Gtk::VBox
{
public:
	AUPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>);
	~AUPluginUI ();

	gint get_preferred_width () { return req_width; }
	gint get_preferred_height () { return req_height; }
	bool start_updating(GdkEventAny*);
	bool stop_updating(GdkEventAny*);

	void activate ();
	void deactivate ();

	bool non_gtk_gui() const { return true; }

	void lower_box_realized ();
	bool lower_box_visibility_notify (GdkEventVisibility*);

	void lower_box_map ();
	void lower_box_unmap ();
	void lower_box_size_request (GtkRequisition*);
	void lower_box_size_allocate (Gtk::Allocation&);

	void cocoa_view_resized ();
	void on_realize ();
	void grab_focus();
	void forward_key_event (GdkEventKey*);

	bool on_window_show (const std::string& /*title*/);
	void on_window_hide ();

	OSStatus carbon_event (EventHandlerCallRef nextHandlerRef, EventRef event);

	void start_live_resize ();
	void end_live_resize ();

private:
	WindowRef wr;
	boost::shared_ptr<ARDOUR::AUPlugin> au;
	int prefheight;
	int prefwidth;

	Gtk::HBox     top_box;
	Gtk::EventBox low_box;
	Gtk::VBox vpacker;
	Gtk::Label automation_mode_label;
	Gtk::ComboBoxText automation_mode_selector;
	Gtk::Label preset_label;

	static std::vector<std::string> automation_mode_strings;

	bool resizable;
	int  req_width;
	int  req_height;

	/* Cocoa */

	NSWindow*           cocoa_window;
	NSView*             au_view;
	NSRect              last_au_frame;
	bool                in_live_resize;
	uint32_t            plugin_requested_resize;

	/* Carbon */

	NSWindow*            cocoa_parent;
	ComponentDescription carbon_descriptor;
	AudioUnitCarbonView  editView;
	WindowRef            carbon_window;
	EventHandlerRef      carbon_event_handler;
	bool                 _activating_from_app;

	/* Generic */

	NotificationObject* _notify;
	LiveResizeNotificationObject* _resize_notify;

	bool test_cocoa_view_support ();
	bool test_carbon_view_support ();
	int  create_carbon_view ();
	int  create_cocoa_view ();

	int parent_carbon_window ();
	int parent_cocoa_window ();
	NSWindow* get_nswindow();

	void update_view_size ();

	bool plugin_class_valid (Class pluginClass);

	friend void au_cf_timer_callback (CFRunLoopTimerRef timer, void* info);
	static CFRunLoopTimerRef   cf_timer;
	static void cf_timer_callback ();
	static int64_t last_timer;
	static bool timer_needed;
	static uint64_t timer_callbacks;
	static uint64_t timer_out_of_range;

	static bool timer_callback ();
	static sigc::connection timer_connection;

public:
	static void start_cf_timer ();
	static void stop_cf_timer ();
};

#endif /* __gtk2_ardour_auplugin_ui_h__  */
