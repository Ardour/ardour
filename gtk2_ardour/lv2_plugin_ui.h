/*
    Copyright (C) 2008 Paul Davis
    Author: Dave Robillard

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

#ifndef __ardour_lv2_plugin_ui_h__
#define __ardour_lv2_plugin_ui_h__

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <vector>
#include <map>
#include <list>

#include <sigc++/signal.h>
#include <gtkmm/widget.h>

#include <ardour_dialog.h>
#include "ardour/types.h"
#include "plugin_ui.h"

#ifdef HAVE_SLV2

namespace ARDOUR {
	class PluginInsert;
	class LV2Plugin;
}

class LV2PluginUI : public PlugUIBase, public Gtk::VBox
{
  public:
	LV2PluginUI (boost::shared_ptr<ARDOUR::PluginInsert>, boost::shared_ptr<ARDOUR::LV2Plugin>);
	~LV2PluginUI ();

	gint get_preferred_height ();
	gint get_preferred_width ();
	bool start_updating(GdkEventAny*);
	bool stop_updating(GdkEventAny*);

	int package (Gtk::Window&);

  private:
	boost::shared_ptr<ARDOUR::LV2Plugin> _lv2;
	std::vector<int> _output_ports;
	sigc::connection _screen_update_connection;
	
	Gtk::Widget*   _gui_widget;
	SLV2UIInstance _inst;
	float*         _values;
	
	static void lv2_ui_write(
			LV2UI_Controller controller,
			uint32_t         port_index,
			uint32_t         buffer_size,
			uint32_t         format,
			const void*      buffer);
	
	void parameter_changed(uint32_t, float);
	void parameter_update(uint32_t, float);
	bool configure_handler (GdkEventConfigure*);
	void save_plugin_setting ();
	void output_update();
	bool is_update_wanted(uint32_t index);
};

#endif // HAVE_SLV2

#endif /* __ardour_lv2_plugin_ui_h__ */

