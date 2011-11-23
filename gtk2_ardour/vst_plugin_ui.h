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

#ifndef __ardour_vst_plugin_ui_h__
#define __ardour_vst_plugin_ui_h__

#include "plugin_ui.h"

namespace ARDOUR {
	class VSTPlugin;
}

class VSTPluginUI : public PlugUIBase, public Gtk::VBox
{
public:
	VSTPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>, boost::shared_ptr<ARDOUR::VSTPlugin>);
	virtual ~VSTPluginUI ();

	virtual int get_preferred_height ();
	virtual int get_preferred_width ();

	virtual int package (Gtk::Window &);

	bool non_gtk_gui () const { return true; }
	
protected:

	virtual int get_XID () = 0;
	
	boost::shared_ptr<ARDOUR::VSTPlugin> _vst;
	Gtk::Socket _socket;

private:

	bool configure_handler (GdkEventConfigure *);
	void preset_selected ();
};

#endif
