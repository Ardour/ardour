/*
 * Copyright (C) 2008-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_lv2_plugin_ui_h__
#define __ardour_lv2_plugin_ui_h__

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <list>
#include <map>
#include <set>
#include <vector>

#include <gtkmm/widget.h>
#include <sigc++/signal.h>

#include "ardour_dialog.h"
#include "ardour/types.h"
#include "plugin_ui.h"

#include "ardour/plugin_insert.h"

#include "lv2_external_ui.h"

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

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
	void grab_focus ();

private:

	void control_changed (uint32_t);

	typedef boost::shared_ptr<ARDOUR::AutomationControl> ControllableRef;

	boost::shared_ptr<ARDOUR::PluginInsert> _pi;
	boost::shared_ptr<ARDOUR::LV2Plugin> _lv2;
	std::vector<int>                     _output_ports;
	sigc::connection                     _screen_update_connection;
	sigc::connection                     _message_update_connection;
	Gtk::Widget*                         _gui_widget;
	/** a box containing the focus, bypass, delete, save / add preset buttons etc. */
	Gtk::HBox                            _ardour_buttons_box;
	float*                               _values_last_sent_to_ui;
	std::vector<ControllableRef>         _controllables;
	struct lv2_external_ui_host          _external_ui_host;
	LV2_Feature                          _external_ui_feature;
	LV2_Feature                          _external_kxui_feature;
#ifdef HAVE_LV2_1_17_2
	LV2UI_Request_Value                  _lv2ui_request_value;
	LV2_Feature                          _lv2ui_request_feature;
#endif
	struct lv2_external_ui*              _external_ui_ptr;
	LV2_Feature                          _parent_feature;
	void*                                _inst;
	typedef std::set<uint32_t> Updates;
	Updates                              _updates;

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

	static uint32_t port_index(void* controller, const char* symbol);

	static void touch(void*    controller,
	                  uint32_t port_index,
	                  bool     grabbed);

#ifdef HAVE_LV2_1_17_2
	static LV2UI_Request_Value_Status
	request_value(void*                     handle,
	              LV2_URID                  key,
	              LV2_URID                  type,
	              const LV2_Feature* const* features);
#endif

	void set_path_property (int,
	                        const ARDOUR::ParameterDescriptor&,
	                        Gtk::FileChooserDialog*);
	std::set<uint32_t> active_parameter_requests;

	void update_timeout();

	void lv2ui_instantiate(const std::string& title);
	void lv2ui_free();

	void parameter_update(uint32_t, float);
	bool configure_handler (GdkEventConfigure*);
	void save_plugin_setting ();
	void output_update();
	void queue_port_update();
	bool is_update_wanted(uint32_t index);

	virtual bool on_window_show(const std::string& title);
	virtual void on_window_hide();
};

#endif /* __ardour_lv2_plugin_ui_h__ */
