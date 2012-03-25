/*
    Copyright (C) 2008-2012 Paul Davis
    Author: David Robillard

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

#include <list>
#include <map>
#include <vector>

#include <gtkmm/widget.h>
#include <sigc++/signal.h>

#include "ardour_dialog.h"
#include "ardour/types.h"
#include "plugin_ui.h"

#ifdef LV2_SUPPORT

#include "lv2_external_ui.h"

namespace ARDOUR {
	class PluginInsert;
	class LV2Plugin;
}

class LV2PluginUI : public PlugUIBase, public Gtk::VBox
{
  public:
	LV2PluginUI (boost::shared_ptr<ARDOUR::PluginInsert>,
	             boost::shared_ptr<ARDOUR::LV2Plugin>);
	~LV2PluginUI ();

	gint get_preferred_height ();
	gint get_preferred_width ();
	bool resizable ();

	bool start_updating(GdkEventAny*);
	bool stop_updating(GdkEventAny*);

	int package (Gtk::Window&);

  private:

	void parameter_changed (uint32_t, float);

	typedef boost::shared_ptr<ARDOUR::AutomationControl> ControllableRef;

	boost::shared_ptr<ARDOUR::LV2Plugin> _lv2;
	std::vector<int>                     _output_ports;
	sigc::connection                     _screen_update_connection;
	Gtk::Widget*                         _gui_widget;
	/** a box containing the focus, bypass, delete, save / add preset buttons etc. */
	Gtk::HBox*                           _ardour_buttons_box;
	float*                               _values;
	std::vector<ControllableRef>         _controllables;
	struct lv2_external_ui_host          _external_ui_host;
	LV2_Feature                          _external_ui_feature;
	struct lv2_external_ui*              _external_ui_ptr;
	Gtk::Window*                         _win_ptr;
	void*                                _inst;

	static void on_external_ui_closed(void* controller);

	static void write_from_ui(void*       controller,
	                          uint32_t    port_index,
	                          uint32_t    buffer_size,
	                          uint32_t    format,
	                          const void* buffer);

	static void write_to_ui(void*       controller,
	                        uint32_t    port_index,
	                        uint32_t    buffer_size,
	                        uint32_t    format,
	                        const void* buffer);

	void update_timeout();

	void lv2ui_instantiate(const std::string& title);
	void lv2ui_free();

	void parameter_update(uint32_t, float);
	bool configure_handler (GdkEventConfigure*);
	void save_plugin_setting ();
	void output_update();
	bool is_update_wanted(uint32_t index);

	virtual bool on_window_show(const std::string& title);
	virtual void on_window_hide();
};

#endif // LV2_SUPPORT

#endif /* __ardour_lv2_plugin_ui_h__ */

