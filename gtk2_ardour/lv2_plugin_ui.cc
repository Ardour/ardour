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

void
LV2PluginUI::lv2_ui_write(
		LV2UI_Controller controller,
		uint32_t         port_index,
		uint32_t         /*buffer_size*/,
		uint32_t         /*format*/,
		const void*      buffer)
{
	LV2PluginUI* me = (LV2PluginUI*)controller;
	if (*(float*)buffer != me->_values[port_index])
		me->_lv2->set_parameter(port_index, *(float*)buffer);
}

void
LV2PluginUI::parameter_changed (uint32_t port_index, float val)
{
	if (val != _values[port_index]) {
		parameter_update(port_index, val);
	}
}

void
LV2PluginUI::parameter_update (uint32_t port_index, float val)
{
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
	if (!_output_ports.empty()) {
		_screen_update_connection.disconnect();
	}
	return false;
}

void
LV2PluginUI::output_update()
{
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
{
	_inst = slv2_ui_instantiate(
			_lv2->slv2_plugin(), _lv2->slv2_ui(), LV2PluginUI::lv2_ui_write, this,
			_lv2->features());
			
	uint32_t num_ports = slv2_plugin_get_num_ports(lv2p->slv2_plugin());
	for (uint32_t i = 0; i < num_ports; ++i) {
		if (lv2p->parameter_is_output(i) && lv2p->parameter_is_control(i) && is_update_wanted(i)) {
			_output_ports.push_back(i);
		}
	}
	
	GtkWidget* c_widget = (GtkWidget*)slv2_ui_instance_get_widget(_inst);
	_gui_widget = Glib::wrap(c_widget);
	_gui_widget->show_all();
	pack_start(*_gui_widget, true, true);
	
	_values = new float[num_ports];
	for (uint32_t i = 0; i < num_ports; ++i) {
		bool ok;
		uint32_t port = lv2p->nth_parameter(i, ok);
		if (ok) {
			_values[port] = lv2p->get_parameter(port);
			if (lv2p->parameter_is_control(port) && lv2p->parameter_is_input(port)) {
				parameter_update(port, _values[port]);
			}
		}
	}
		
	_lv2->ParameterChanged.connect(mem_fun(*this, &LV2PluginUI::parameter_changed));
}

LV2PluginUI::~LV2PluginUI ()
{
	delete[] _values;
	// plugin destructor destroys the GUI
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
	/* forward configure events to plugin window */
	win.signal_configure_event().connect (mem_fun (*this, &LV2PluginUI::configure_handler));
	win.signal_map_event().connect (mem_fun (*this, &LV2PluginUI::start_updating));
	win.signal_unmap_event().connect (mem_fun (*this, &LV2PluginUI::stop_updating));
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
