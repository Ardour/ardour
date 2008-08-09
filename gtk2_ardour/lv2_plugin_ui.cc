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

#include <ardour/insert.h>
#include <ardour/lv2_plugin.h>

#include "lv2_plugin_ui.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

void
LV2PluginUI::lv2_ui_write(LV2UI_Controller controller,
             uint32_t         port_index,
             uint32_t         buffer_size,
             uint32_t         format,
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
		const LV2UI_Descriptor* ui_desc = slv2_ui_instance_get_descriptor(_inst);
		LV2UI_Handle ui_handle = slv2_ui_instance_get_handle(_inst);
		if (ui_desc->port_event)
			ui_desc->port_event(ui_handle, port_index, 4, 0, &val);
		_values[port_index] = val;
	}
}

LV2PluginUI::LV2PluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<LV2Plugin> lv2p)
	: PlugUIBase (pi)
	, _lv2(lv2p)
{
	_inst = slv2_ui_instantiate(
			_lv2->slv2_plugin(), _lv2->slv2_ui(), LV2PluginUI::lv2_ui_write, this,
			/* FEATURES */ NULL);
			
	GtkWidget* c_widget = (GtkWidget*)slv2_ui_instance_get_widget(_inst);
	_gui_widget = Glib::wrap(c_widget);
	_gui_widget->show_all();
	pack_start(*_gui_widget, true, true);
	
	uint32_t num_ports = slv2_plugin_get_num_ports(lv2p->slv2_plugin());
	_values = new float[num_ports];
	for (uint32_t i = 0; i < num_ports; ++i) {
		bool ok;
		_values[i] = lv2p->nth_parameter(i, ok);
		if (ok)
			lv2_ui_write(this, i, 4, /* FIXME: format */0, &_values[i]);
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
	return 0;
}

bool
LV2PluginUI::configure_handler (GdkEventConfigure* ev)
{
	cout << "CONFIGURE" << endl;
	return false;
}

