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

#include "ardour/lv2_plugin.h"
#include "ardour/session.h"
#include "pbd/error.h"

#include "ardour_ui.h"
#include "lv2_plugin_ui.h"

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include <lilv/lilv.h>
#include <suil/suil.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace PBD;

#define NS_UI "http://lv2plug.in/ns/extensions/ui#"

static SuilHost* ui_host = NULL;

void
LV2PluginUI::write_from_ui(void*       controller,
                           uint32_t    port_index,
                           uint32_t    buffer_size,
                           uint32_t    format,
                           const void* buffer)
{
	LV2PluginUI* me = (LV2PluginUI*)controller;
	if (format == 0) {
		if (port_index >= me->_controllables.size()) {
			return;
		}

		boost::shared_ptr<AutomationControl> ac = me->_controllables[port_index];
		if (ac) {
			ac->set_value(*(const float*)buffer);
		}
	} else if (format == me->_lv2->urids.atom_eventTransfer) {

		const int cnt = me->_pi->get_count();
		for (int i=0; i < cnt; i++ ) {
			boost::shared_ptr<LV2Plugin> lv2i = boost::dynamic_pointer_cast<LV2Plugin> (me->_pi->plugin(i));
			lv2i->write_from_ui(port_index, format, buffer_size, (const uint8_t*)buffer);
		}
	}
}

void
LV2PluginUI::write_to_ui(void*       controller,
                         uint32_t    port_index,
                         uint32_t    buffer_size,
                         uint32_t    format,
                         const void* buffer)
{
	LV2PluginUI* me = (LV2PluginUI*)controller;
	if (me->_inst) {
		suil_instance_port_event((SuilInstance*)me->_inst,
					 port_index, buffer_size, format, buffer);
	}
}

uint32_t
LV2PluginUI::port_index(void* controller, const char* symbol)
{
	return ((LV2PluginUI*)controller)->_lv2->port_index(symbol);
}

void
LV2PluginUI::touch(void*    controller,
                   uint32_t port_index,
                   bool     grabbed)
{
	LV2PluginUI* me = (LV2PluginUI*)controller;
	if (port_index >= me->_controllables.size()) {
		return;
	}

	ControllableRef control = me->_controllables[port_index];
	if (grabbed) {
		control->start_touch(control->session().transport_frame());
	} else {
		control->stop_touch(false, control->session().transport_frame());
	}
}

void
LV2PluginUI::update_timeout()
{
	_lv2->emit_to_ui(this, &LV2PluginUI::write_to_ui);
}

void
LV2PluginUI::on_external_ui_closed(void* controller)
{
	LV2PluginUI* me = (LV2PluginUI*)controller;
	me->_screen_update_connection.disconnect();
	me->_external_ui_ptr = NULL;
}

void
LV2PluginUI::parameter_changed(uint32_t port_index, float val)
{
	PlugUIBase::parameter_changed(port_index, val);

	if (val != _values[port_index]) {
		parameter_update(port_index, val);
	}
}

void
LV2PluginUI::parameter_update(uint32_t port_index, float val)
{
	if (!_inst) {
		return;
	}

	suil_instance_port_event((SuilInstance*)_inst, port_index, 4, 0, &val);
	_values[port_index] = val;
}

bool
LV2PluginUI::start_updating(GdkEventAny*)
{
	if (!_output_ports.empty()) {
		_screen_update_connection.disconnect();
		_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect
		        (sigc::mem_fun(*this, &LV2PluginUI::output_update));
	}
	return false;
}

bool
LV2PluginUI::stop_updating(GdkEventAny*)
{
	//cout << "stop_updating" << endl;

	if (!_output_ports.empty()) {
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

LV2PluginUI::LV2PluginUI(boost::shared_ptr<PluginInsert> pi,
                         boost::shared_ptr<LV2Plugin>    lv2p)
	: PlugUIBase(pi)
	, _pi(pi)
	, _lv2(lv2p)
	, _gui_widget(NULL)
	, _ardour_buttons_box(NULL)
	, _values(NULL)
	, _external_ui_ptr(NULL)
	, _inst(NULL)
{
}

void
LV2PluginUI::lv2ui_instantiate(const std::string& title)
{
	bool          is_external_ui = _lv2->is_external_ui();
	LV2_Feature** features_src   = const_cast<LV2_Feature**>(_lv2->features());
	LV2_Feature** features       = const_cast<LV2_Feature**>(_lv2->features());
	size_t        features_count = 0;
	while (*features++) {
		features_count++;
	}

	Gtk::Alignment* container = NULL;
	if (is_external_ui) {
		_external_ui_host.ui_closed       = LV2PluginUI::on_external_ui_closed;
		_external_ui_host.plugin_human_id = strdup(title.c_str());

		_external_ui_feature.URI  = LV2_EXTERNAL_UI_URI;
		_external_ui_feature.data = &_external_ui_host;

		++features_count;
		features = (LV2_Feature**)malloc(
			sizeof(LV2_Feature*) * (features_count + 1));
		for (size_t i = 0; i < features_count - 1; ++i) {
			features[i] = features_src[i];
		}
		features[features_count - 1] = &_external_ui_feature;
		features[features_count]     = NULL;
	} else {
		_ardour_buttons_box = manage (new Gtk::HBox);
		_ardour_buttons_box->set_spacing (6);
		_ardour_buttons_box->set_border_width (6);
		_ardour_buttons_box->pack_end (focus_button, false, false);
		_ardour_buttons_box->pack_end (bypass_button, false, false, 10);
		_ardour_buttons_box->pack_end (delete_button, false, false);
		_ardour_buttons_box->pack_end (save_button, false, false);
		_ardour_buttons_box->pack_end (add_button, false, false);
		_ardour_buttons_box->pack_end (_preset_combo, false, false);
		_ardour_buttons_box->pack_end (_preset_modified, false, false);
		_ardour_buttons_box->show_all();
		pack_start(*_ardour_buttons_box, false, false);

		_gui_widget = Gtk::manage((container = new Gtk::Alignment()));
		pack_start(*_gui_widget, true, true);
		_gui_widget->show();

		_parent_feature.URI  = LV2_UI__parent;
		_parent_feature.data = _gui_widget->gobj();

		++features_count;
		features = (LV2_Feature**)malloc(
			sizeof(LV2_Feature*) * (features_count + 1));
		for (size_t i = 0; i < features_count - 1; ++i) {
			features[i] = features_src[i];
		}
		features[features_count - 1] = &_parent_feature;
		features[features_count]     = NULL;
	}

	if (!ui_host) {
		ui_host = suil_host_new(LV2PluginUI::write_from_ui,
		                        LV2PluginUI::port_index,
		                        NULL, NULL);
		suil_host_set_touch_func(ui_host, LV2PluginUI::touch);
	}
	const char* container_type = (is_external_ui)
		? NS_UI "external"
		: NS_UI "GtkUI";

	const LilvUI* ui = (const LilvUI*)_lv2->c_ui();
	_inst = suil_instance_new(
		ui_host,
		this,
		container_type,
		_lv2->uri(),
		lilv_node_as_uri(lilv_ui_get_uri(ui)),
		lilv_node_as_uri((const LilvNode*)_lv2->c_ui_type()),
		lilv_uri_to_path(lilv_node_as_uri(lilv_ui_get_bundle_uri(ui))),
		lilv_uri_to_path(lilv_node_as_uri(lilv_ui_get_binary_uri(ui))),
		features);

	free(features);

#define GET_WIDGET(inst) suil_instance_get_widget((SuilInstance*)inst);

	const uint32_t num_ports = _lv2->num_ports();
	for (uint32_t i = 0; i < num_ports; ++i) {
		if (_lv2->parameter_is_output(i)
		    && _lv2->parameter_is_control(i)
		    && is_update_wanted(i)) {
			_output_ports.push_back(i);
		}
	}

	_external_ui_ptr = NULL;
	if (_inst) {
		if (!is_external_ui) {
			GtkWidget* c_widget = (GtkWidget*)GET_WIDGET(_inst);
			if (!c_widget) {
				error << _("failed to get LV2 UI widget") << endmsg;
				suil_instance_free((SuilInstance*)_inst);
				_inst = NULL;
				return;
			}
			if (!container->get_child()) {
				// Suil didn't add the UI to the container for us, so do it now
				container->add(*Gtk::manage(Glib::wrap(c_widget)));
			}
			container->show_all();
		} else {
			_external_ui_ptr = (struct lv2_external_ui*)GET_WIDGET(_inst);
		}
	}

	_values = new float[num_ports];
	_controllables.resize(num_ports);
	for (uint32_t i = 0; i < num_ports; ++i) {
		bool     ok;
		uint32_t port = _lv2->nth_parameter(i, ok);
		if (ok) {
			_values[port]        = _lv2->get_parameter(port);
			_controllables[port] = boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (
				insert->control(Evoral::Parameter(PluginAutomation, 0, port)));

			if (_lv2->parameter_is_control(port) && _lv2->parameter_is_input(port)) {
				parameter_update(port, _values[port]);
			}
		}
	}

	if (_lv2->has_message_output()) {
		_lv2->enable_ui_emmission();
		ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect(
			sigc::mem_fun(*this, &LV2PluginUI::update_timeout));
	}
}

void
LV2PluginUI::lv2ui_free()
{
	stop_updating (0);

	if (_gui_widget) {
		remove (*_gui_widget);
		_gui_widget = NULL;
	}

	if (_inst) {
		suil_instance_free((SuilInstance*)_inst);
		_inst = NULL;
	}
}

LV2PluginUI::~LV2PluginUI ()
{
	if (_values) {
		delete[] _values;
	}

	/* Close and delete GUI. */
	lv2ui_free();

	_screen_update_connection.disconnect();

	if (_lv2->is_external_ui()) {
		/* External UI is no longer valid.
		   on_window_hide() will not try to use it if is NULL.
		*/
		_external_ui_ptr = NULL;
	}
}

int
LV2PluginUI::get_preferred_height()
{
	Gtk::Requisition r = size_request();
	return r.height;
}

int
LV2PluginUI::get_preferred_width()
{
	Gtk::Requisition r = size_request();
	return r.width;
}

bool
LV2PluginUI::resizable()
{
	return _lv2->ui_is_resizable();
}

int
LV2PluginUI::package(Gtk::Window& win)
{
	if (_external_ui_ptr) {
		_win_ptr = &win;
	} else {
		/* forward configure events to plugin window */
		win.signal_configure_event().connect(
			sigc::mem_fun(*this, &LV2PluginUI::configure_handler));
		win.signal_map_event().connect(
			sigc::mem_fun(*this, &LV2PluginUI::start_updating));
		win.signal_unmap_event().connect(
			sigc::mem_fun(*this, &LV2PluginUI::stop_updating));
	}
	return 0;
}

bool
LV2PluginUI::configure_handler(GdkEventConfigure*)
{
	std::cout << "CONFIGURE" << std::endl;
	return false;
}

bool
LV2PluginUI::is_update_wanted(uint32_t /*index*/)
{
	/* FIXME: use port notification properties
	   and/or new UI extension subscription methods
	*/
	return true;
}

bool
LV2PluginUI::on_window_show(const std::string& title)
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
		_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect
		        (sigc::mem_fun(*this, &LV2PluginUI::output_update));
		return false;
	} else {
		lv2ui_instantiate("gtk2gui");
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
	} else {
		lv2ui_free();
	}
}
