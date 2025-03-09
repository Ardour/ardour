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

#ifndef __ardour_vst3_x11_plugin_ui_h__
#define __ardour_vst3_x11_plugin_ui_h__

#ifdef VST3_SUPPORT

#include <ytkmm/widget.h>
#include <ytkmm/socket.h>

#include "vst3_plugin_ui.h"

class VST3X11PluginUI : public VST3PluginUI
{
public:
	VST3X11PluginUI (std::shared_ptr<ARDOUR::PlugInsertBase>, std::shared_ptr<ARDOUR::VST3Plugin>);
	~VST3X11PluginUI ();

	bool on_window_show(const std::string&);
	void on_window_hide ();
	void grab_focus();

private:
	void view_realized ();
	void view_size_request (GtkRequisition*);
	void view_size_allocate (Gtk::Allocation&);
	void resize_callback (int, int);

	Gtk::Socket _gui_widget;

};

#endif // VST3_SUPPORT
#endif
