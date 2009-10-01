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

#include "ardour/processor.h"
#include "ardour/lv2_plugin.h"

#include "ardour_ui.h"
#include "lv2_plugin_ui.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

std::vector<struct lv2_external_ui*> g_external_uis;

void close_external_ui_windows()
{
	struct lv2_external_ui* external_ui_ptr;

	//cout << "close_external_ui_windows" << endl;

	while (!g_external_uis.empty()) {
		//cout << "pop" << endl;
		external_ui_ptr = g_external_uis.back();
		LV2_EXTERNAL_UI_HIDE(external_ui_ptr);
		g_external_uis.pop_back();
	}
}

void
LV2PluginUI::lv2_ui_write(
		LV2UI_Controller controller,
		uint32_t         port_index,
		uint32_t         /*buffer_size*/,
		uint32_t         /*format*/,
		const void*      buffer)
{
	//cout << "lv2_ui_write" << endl;
	LV2PluginUI* me = (LV2PluginUI*)controller;
	if (*(float*)buffer != me->_values[port_index]) {
		//cout << "set_parameter " << port_index << ":"  << *(float*)buffer << endl;
		me->_lv2->set_parameter(port_index, *(float*)buffer);
  }
}

void LV2PluginUI::on_external_ui_closed(LV2UI_Controller controller)
{
	//cout << "on_external_ui_closed" << endl;

	LV2PluginUI* me = (LV2PluginUI*)controller;
	me->_screen_update_connection.disconnect();
	//me->insert->set_gui(0);

	for (vector<struct lv2_external_ui*>::iterator it = g_external_uis.begin() ; it < g_external_uis.end(); it++) {
		if (*it == me->_external_ui_ptr) {
			g_external_uis.erase(it);
		}
	}

	//slv2_ui_instance_get_descriptor(me->_inst)->cleanup(me->_inst);
	me->_external_ui_ptr = NULL;
}

void
LV2PluginUI::parameter_changed (uint32_t port_index, float val)
{
	//cout << "parameter_changed" << endl;
	if (val != _values[port_index]) {
		parameter_update(port_index, val);
	}
}

void
LV2PluginUI::parameter_update (uint32_t port_index, float val)
{
	if (!_inst) {
		return;
	}

	const LV2UI_Descriptor* ui_desc = slv2_ui_instance_get_descriptor(_inst);
	LV2UI_Handle ui_handle = slv2_ui_instance_get_handle(_inst);
	if (ui_desc->port_event)
		ui_desc->port_event(ui_handle, port_index, 4, 0, &val);
	_values[port_index] = val;
}

bool
LV2PluginUI::start_updating(GdkEventAny*)
{
	if (!_output_ports.empty()) {
		_screen_update_connection.disconnect();
		_screen_update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect 
			(mem_fun(*this, &LV2PluginUI::output_update));
	}
	return false;
}

bool
LV2PluginUI::stop_updating(GdkEventAny*)
{
	//cout << "stop_updating" << endl;

	if (//!_external_ui_ptr &&
			!_output_ports.empty()) {
		_screen_update_connection.disconnect();
	}
	return false;
}

void
LV2PluginUI::output_update()
{
	//cout << "output_update" << endl;
	if (_external_ui_ptr) {
		LV2_EXTERNAL_UI_RUN(_external_ui_ptr);
	}

	/* FIXME only works with control output ports (which is all we support now anyway) */
	uint32_t nports = _output_ports.size();
	for (uint32_t i = 0; i < nports; ++i) {
		uint32_t index = _output_ports[i];
		parameter_changed(index, _lv2->get_parameter(index));
	}
	
}

LV2PluginUI::LV2PluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<LV2Plugin> lv2p)
	: PlugUIBase (pi)
	, _lv2(lv2p)
	, _inst(NULL)
	, _values(NULL)
	, _external_ui_ptr(NULL)
{
	if (!_lv2->is_external_ui()) {
		lv2ui_instantiate("gtk2gui");
	}
}

void
LV2PluginUI::lv2ui_instantiate(const Glib::ustring& title)
{
	LV2_Feature** features;
	LV2_Feature** features_src;
	LV2_Feature** features_dst;
	size_t features_count;
	bool is_external_ui;

	is_external_ui = _lv2->is_external_ui();

	if (is_external_ui) {
		_external_ui_host.ui_closed = LV2PluginUI::on_external_ui_closed;
		_external_ui_host.plugin_human_id = strdup(title.c_str());

		_external_ui_feature.URI = LV2_EXTERNAL_UI_URI;
		_external_ui_feature.data = &_external_ui_host;

		features_src = features = (LV2_Feature**)_lv2->features();
		features_count = 2;
		while (*features++) {
			features_count++;
		}

		features_dst = features = (LV2_Feature**)malloc(sizeof(LV2_Feature*) * features_count);
		features_dst[--features_count] = NULL;
		features_dst[--features_count] = &_external_ui_feature;
		while (features_count--) {
			*features++ = *features_src++;
		}
	} else {
		features_dst = (LV2_Feature**)_lv2->features();
	}

	_inst = slv2_ui_instantiate(
			_lv2->slv2_plugin(), _lv2->slv2_ui(), LV2PluginUI::lv2_ui_write, this,
			features_dst);

	if (is_external_ui) {
		free(features_dst);
	}
			
	uint32_t num_ports = slv2_plugin_get_num_ports(_lv2->slv2_plugin());
	for (uint32_t i = 0; i < num_ports; ++i) {
		if (_lv2->parameter_is_output(i) && _lv2->parameter_is_control(i) && is_update_wanted(i)) {
			_output_ports.push_back(i);
		}
	}

	_external_ui_ptr = NULL;
	if (_inst) {
		if (!is_external_ui) {
			GtkWidget* c_widget = (GtkWidget*)slv2_ui_instance_get_widget(_inst);
			_gui_widget = Glib::wrap(c_widget);
			_gui_widget->show_all();
			pack_start(*_gui_widget, true, true);
		} else {
			_external_ui_ptr = (struct lv2_external_ui *)slv2_ui_instance_get_widget(_inst);
			g_external_uis.push_back(_external_ui_ptr);
		}
	}
	
	_values = new float[num_ports];
	for (uint32_t i = 0; i < num_ports; ++i) {
		bool ok;
		uint32_t port = _lv2->nth_parameter(i, ok);
		if (ok) {
			_values[port] = _lv2->get_parameter(port);
			if (_lv2->parameter_is_control(port) && _lv2->parameter_is_input(port)) {
				parameter_update(port, _values[port]);
			}
		}
	}
		
	_lv2->ParameterChanged.connect(mem_fun(*this, &LV2PluginUI::parameter_changed));
}

LV2PluginUI::~LV2PluginUI ()
{
	//cout << "LV2PluginUI destructor called" << endl;

	if (_values) {
		delete[] _values;
	}
	// plugin destructor destroys the GTK GUI
}

int
LV2PluginUI::get_preferred_height ()
{
	Gtk::Requisition r = size_request();
	return r.height;
}

int
LV2PluginUI::get_preferred_width ()
{
	Gtk::Requisition r = size_request();
	return r.width;
}

int
LV2PluginUI::package (Gtk::Window& win)
{
	//cout << "package" << endl;
	if (_external_ui_ptr) {
		_win_ptr = &win;
	} else {
		/* forward configure events to plugin window */
		win.signal_configure_event().connect (mem_fun (*this, &LV2PluginUI::configure_handler));
		win.signal_map_event().connect (mem_fun (*this, &LV2PluginUI::start_updating));
		win.signal_unmap_event().connect (mem_fun (*this, &LV2PluginUI::stop_updating));
	}
	return 0;
}

bool
LV2PluginUI::configure_handler (GdkEventConfigure*)
{
	std::cout << "CONFIGURE" << std::endl;
	return false;
}

bool
LV2PluginUI::is_update_wanted(uint32_t /*index*/)
{
	/* FIXME this should check the port notification properties, which nobody sets now anyway :) */
	return true;
}

bool
LV2PluginUI::on_window_show(const Glib::ustring& title)
{
	//cout << "on_window_show - " << title << endl; flush(cout);

	if (_lv2->is_external_ui()) {
		if (_external_ui_ptr) {
			LV2_EXTERNAL_UI_SHOW(_external_ui_ptr);
			return false;
		}
		lv2ui_instantiate(title);
		if (!_external_ui_ptr) {
			return false;
		}

		LV2_EXTERNAL_UI_SHOW(_external_ui_ptr);
		_screen_update_connection.disconnect();
		_screen_update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect 
			(mem_fun(*this, &LV2PluginUI::output_update));
		return false;
	}

	return true;
}

void
LV2PluginUI::on_window_hide()
{
	//cout << "on_window_hide" << endl; flush(cout);

	if (_external_ui_ptr) {
		LV2_EXTERNAL_UI_HIDE(_external_ui_ptr);
		//slv2_ui_instance_get_descriptor(_inst)->cleanup(_inst);
		//_external_ui_ptr = NULL;
		//_screen_update_connection.disconnect();
	}
}
