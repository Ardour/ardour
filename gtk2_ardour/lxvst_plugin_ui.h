/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __lxvst_plugin_ui_h__
#define __lxvst_plugin_ui_h__

#include <sigc++/signal.h>
#include "vst_plugin_ui.h"

#ifdef LXVST_SUPPORT

namespace ARDOUR {
	class PluginInsert;
	class LXVSTPlugin;
}

class LXVSTPluginUI : public VSTPluginUI
{
  public:
	LXVSTPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>, boost::shared_ptr<ARDOUR::VSTPlugin>);
	~LXVSTPluginUI ();

	int get_preferred_height ();
	
	bool start_updating (GdkEventAny *);
	bool stop_updating (GdkEventAny *);

	int package (Gtk::Window&);
	void forward_key_event (GdkEventKey *);
	bool non_gtk_gui () const { return true; }

private:
	void resize_callback ();
	int get_XID ();

	sigc::connection _screen_update_connection;
};

#endif //LXVST_SUPPORT

#endif
