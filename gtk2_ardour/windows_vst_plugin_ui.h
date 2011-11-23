/*
    Copyright (C) 2000-2006 Paul Davis

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

#include "vst_plugin_ui.h"

class WindowsVSTPluginUI : public VSTPluginUI
{
public:
	WindowsVSTPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>, boost::shared_ptr<ARDOUR::VSTPlugin>);
	~WindowsVSTPluginUI ();

	gint get_preferred_height ();
	gint get_preferred_width ();
	bool start_updating (GdkEventAny*) { return false; }
	bool stop_updating (GdkEventAny*) { return false; }

	int package (Gtk::Window &);

	void forward_key_event (GdkEventKey *);

private:
	
	int get_XID ();
	
};
