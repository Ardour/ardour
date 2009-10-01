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

#include <vector>
#include <map>
#include <list>

#include <sigc++/signal.h>
#include <gtkmm/widget.h>

#include <ardour_dialog.h>
#include <ardour/types.h>
#include "plugin_ui.h"

#ifdef HAVE_LV2

#include "lv2_external_ui.h"

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

	struct lv2_external_ui_host _external_ui_host;
	LV2_Feature _external_ui_feature;
	struct lv2_external_ui* _external_ui_ptr;
	Gtk::Window* _win_ptr;

	static void on_external_ui_closed(LV2UI_Controller controller);
	
	static void lv2_ui_write(
			LV2UI_Controller controller,
			uint32_t         port_index,
			uint32_t         buffer_size,
			uint32_t         format,
			const void*      buffer);
	
	void lv2ui_instantiate(const Glib::ustring& title);

	void parameter_changed(uint32_t, float);
	void parameter_update(uint32_t, float);
	bool configure_handler (GdkEventConfigure*);
	void save_plugin_setting ();
	void output_update();
	bool is_update_wanted(uint32_t index);

	virtual bool on_window_show(const Glib::ustring& title);
	virtual void on_window_hide();
};
#endif // HAVE_LV2

#endif /* __ardour_lv2_plugin_ui_h__ */

