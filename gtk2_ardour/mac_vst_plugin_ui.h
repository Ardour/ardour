/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

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

#include "vst_plugin_ui.h"

class MacVSTPluginUI;

@interface ResizeNotificationObject : NSObject {
	@private
		MacVSTPluginUI* plugin_ui;
}
@end

class MacVSTPluginUI : public VSTPluginUI
{
public:
	MacVSTPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>, boost::shared_ptr<ARDOUR::VSTPlugin>);
	~MacVSTPluginUI ();

	bool start_updating (GdkEventAny*) { return false; }
	bool stop_updating (GdkEventAny*) { return false; }

	int package (Gtk::Window &);

	void forward_key_event (GdkEventKey *);
	void view_resized ();

protected:
	void lower_box_realized ();
	bool lower_box_visibility_notify (GdkEventVisibility*);
	void lower_box_map ();
	void lower_box_unmap ();
	void lower_box_size_request (GtkRequisition*);
	void lower_box_size_allocate (Gtk::Allocation&);

private:
	int get_XID ();
	bool idle ();
	void set_program ();
	NSWindow* get_nswindow();

	Gtk::EventBox low_box;
	NSView*          _ns_view;
	sigc::connection _idle_connection;
	PBD::ScopedConnection _program_connection;
	ResizeNotificationObject* _resize_notifier;
};
